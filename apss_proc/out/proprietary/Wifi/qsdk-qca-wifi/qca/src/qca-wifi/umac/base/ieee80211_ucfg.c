/*
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* This is the unified configuration file for iw, acfg and netlink cfg, etc. */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */
#include <net/iw_handler.h>

#include <asm/uaccess.h>

#include "if_media.h"
#include "_ieee80211.h"
#include <osif_private.h>
#include <wlan_opts.h>
#include <ieee80211_var.h>
#include "ieee80211_rateset.h"
#include "ieee80211_vi_dbg.h"
#if ATH_SUPPORT_IBSS_DFS
#include <ieee80211_regdmn.h>
#endif
#include "ieee80211_power_priv.h"
#include "../vendor/generic/ioctl/ioctl_vendor_generic.h"
#include <ol_if_athvar.h>
#include <ol_txrx_dbg.h>
#include "if_athvar.h"
#include "if_athproto.h"
#include "base/ieee80211_node_priv.h"
#include "mlme/ieee80211_mlme_priv.h"
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_vdev_if.h>
#endif

#include "target_type.h"
#include "ieee80211_ucfg.h"
#include <dp_extap.h>
#include <ieee80211_acl.h>
#include "ieee80211_mlme_dfs_dispatcher.h"

#include <wlan_son_ucfg_api.h>
#include <wlan_son_pub.h>
#include "rrm/ieee80211_rrm_priv.h"
#include <ieee80211_cfg80211.h>
#include <wlan_mlme_if.h>

#include <qdf_time.h>

#if QCA_AIRTIME_FAIRNESS
#include <wlan_atf_ucfg_api.h>
#endif /* QCA_AIRTIME_FAIRNESS */

#include <wlan_cmn.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#ifdef WLAN_SUPPORT_FILS
#include <wlan_fd_ucfg_api.h>
#include <wlan_fd_utils_api.h>
#endif /* WLAN_SUPPORT_FILS */

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif

#if WLAN_SUPPORT_SPLITMAC
#include <wlan_splitmac.h>
#endif
#ifdef WLAN_SUPPORT_GREEN_AP
#include <wlan_green_ap_ucfg_api.h>
#endif
#if WLAN_CFR_ENABLE
#include <wlan_cfr_utils_api.h>
#include <wlan_cfr_ucfg_api.h>
#endif

#if QCA_SUPPORT_GPR
#include "ieee80211_ioctl_acfg.h"
#endif

#include <wlan_vdev_mgr_utils_api.h>

#define ONEMBPS 1000
#define HIGHEST_BASIC_RATE 24000
#define THREE_HUNDRED_FIFTY_MBPS 350000

#if UMAC_SUPPORT_QUIET
/*
 * Quiet ie enable flag has 3 bits for now.
 * bit0-enable/disable, bit1-single-shot/continuos & bit3-include/skip
 * quiet ie in swba. So the maximum possible value for this flag is 7 for now.
 */
#define MAX_QUIET_ENABLE_FLAG 7
#endif

extern int ol_ath_ucfg_get_user_postion(wlan_if_t vaphandle, u_int32_t aid);
extern int ol_ath_ucfg_get_peer_mumimo_tx_count(wlan_if_t vaphandle, u_int32_t aid);
extern int ol_ath_ucfg_reset_peer_mumimo_tx_count(wlan_if_t vaphandle, u_int32_t aid);
extern int ieee80211_rate_is_valid_basic(struct ieee80211vap *, u_int32_t);
#if WLAN_SER_UTF
extern void wlan_ser_utf_main(struct wlan_objmgr_vdev *vdev, u_int8_t,
                              u_int32_t);
#endif
#if WLAN_SER_DEBUG
extern void wlan_ser_print_history(struct wlan_objmgr_vdev *vdev, u_int8_t,
                             u_int32_t);
#endif
#if SM_ENG_HIST_ENABLE
extern void wlan_mlme_print_all_sm_history(void);
#endif
#if MESH_MODE_SUPPORT
int ieee80211_add_localpeer(wlan_if_t vap, char *params);
int ieee80211_authorise_local_peer(wlan_if_t vap, char *params);
#endif

#if IEEE80211_DEBUG_NODELEAK
void wlan_debug_dump_nodes_tgt(void);
#endif

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
extern void wlan_send_omn_action(void *arg, wlan_node_t node);
extern void ieee80211_autochan_switch_chan_change_csa(struct ieee80211com *ic,
                                                      uint8_t ch_ieee,
                                                      uint16_t ch_width);
#endif

#ifdef QCA_PARTNER_PLATFORM
extern int wlan_pltfrm_set_param(wlan_if_t vaphandle, u_int32_t val);
extern int wlan_pltfrm_get_param(wlan_if_t vaphandle);
#endif
static const int basic_11b_rate[] = {1000, 2000, 5500, 11000 };
static const int basic_11bgn_rate[] = {1000, 2000, 5500, 6000, 11000, 12000, 24000 };
static const int basic_11na_rate[] = {6000, 12000, 24000 };

#define HIGHEST_BASIC_RATE 24000    /* Highest rate that can be used for mgmt frames */

/* Get mu tx blacklisted count for the peer */
void wlan_get_mu_tx_blacklist_cnt(ieee80211_vap_t vap,int value)
{
    struct ieee80211_node *node = NULL;
    u_int16_t aid = 0;
    bool found = FALSE;
    struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
    TAILQ_FOREACH(node, &nt->nt_node, ni_list) {
       if(!ieee80211_try_ref_node(node))
           continue;

       aid = IEEE80211_AID(node->ni_associd);
       ieee80211_free_node(node);
       if (aid == ((u_int16_t)value)) {
           found = TRUE;
           break;
       }
    }
    if (found == FALSE) {
       qdf_print("Invalid AID value.");
    }
}

int ieee80211_ucfg_set_essid(wlan_if_t vap, ieee80211_ssid *data, bool is_vap_restart_required)
{
    osif_dev *osifp = (osif_dev *)vap->iv_ifp;
    struct net_device *dev = osifp->netdev;
    ieee80211_ssid   tmpssid;
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);

    if (osifp->is_delete_in_progress)
        return -EINVAL;

    if (opmode == IEEE80211_M_WDS)
        return -EOPNOTSUPP;

    if(data->len <= 0)
        return -EINVAL;

    OS_MEMZERO(&tmpssid, sizeof(ieee80211_ssid));

    if (data->len > IEEE80211_NWID_LEN)
        data->len = IEEE80211_NWID_LEN;

    tmpssid.len = data->len;
    OS_MEMCPY(tmpssid.ssid, data->ssid, data->len);
    /*
     * Deduct a trailing \0 since iwconfig passes a string
     * length that includes this.  Unfortunately this means
     * that specifying a string with multiple trailing \0's
     * won't be handled correctly.  Not sure there's a good
     * solution; the API is botched (the length should be
     * exactly those bytes that are meaningful and not include
     * extraneous stuff).
     */
     if (data->len > 0 &&
            tmpssid.ssid[data->len-1] == '\0')
        tmpssid.len--;

#ifdef ATH_SUPERG_XR
    if (vap->iv_xrvap != NULL && !(vap->iv_flags & IEEE80211_F_XR))
    {
        copy_des_ssid(vap->iv_xrvap, vap);
    }
#endif

    wlan_set_desired_ssidlist(vap,1,&tmpssid);

    if (tmpssid.len) {
        qdf_nofl_info("DES SSID SET=%s", tmpssid.ssid);
    }


#ifdef ATH_SUPPORT_P2P
    /* For P2P supplicant we do not want start connnection as soon as ssid is set */
    /* The difference in behavior between non p2p supplicant and p2p supplicant need to be fixed */
    /* see EV 73753 for more details */
    if ((osifp->os_opmode == IEEE80211_M_P2P_CLIENT
                || osifp->os_opmode == IEEE80211_M_STA
                || osifp->os_opmode == IEEE80211_M_P2P_GO) && !vap->auto_assoc)
        return 0;
#endif

    if (!is_vap_restart_required) {
        return 0;
    }

    return (IS_UP(dev) &&
            ((osifp->os_opmode == IEEE80211_M_HOSTAP) || /* Call vap init for AP mode if netdev is UP */
             (vap->iv_ic->ic_roaming != IEEE80211_ROAMING_MANUAL))) ? osif_vap_init(dev, RESCAN) : 0;
}

int ieee80211_ucfg_get_essid(wlan_if_t vap, ieee80211_ssid *data, int *nssid)
{
    enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);

    if (opmode == IEEE80211_M_WDS)
        return -EOPNOTSUPP;

    *nssid = wlan_get_desired_ssidlist(vap, data, 1);
    if (*nssid <= 0)
    {
        if (opmode == IEEE80211_M_HOSTAP)
            data->len = 0;
        else
            wlan_get_bss_essid(vap, data);
    }

    return 0;
}

int ieee80211_ucfg_get_freq(wlan_if_t vap)
{
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;
    wlan_chan_t chan;
    int freq;

    if (dev->flags & (IFF_UP | IFF_RUNNING)) {
        chan = wlan_get_bss_channel(vap);
    } else {
        chan = wlan_get_current_channel(vap, true);
    }
    if (chan != IEEE80211_CHAN_ANYC) {
        freq = (int)chan->ic_freq;
    } else {
        freq = 0;
    }

    return freq;
}

int ieee80211_ucfg_set_freq(wlan_if_t vap, int ieeechannel)
{
    int ret;
    struct ieee80211com *ic = vap->iv_ic;

    /* Channel change from user and radar are serialized using IEEE80211_CHANCHANGE_MARKRADAR flag.
     */
    IEEE80211_CHAN_CHANGE_LOCK(ic);
    if (!IEEE80211_CHANCHANGE_STARTED_IS_SET(ic) && !IEEE80211_CHANCHANGE_MARKRADAR_IS_SET(ic)) {
        IEEE80211_CHANCHANGE_STARTED_SET(ic);
        IEEE80211_CHANCHANGE_MARKRADAR_SET(ic);
        IEEE80211_CHAN_CHANGE_UNLOCK(ic);
        ret = ieee80211_ucfg_set_freq_internal(vap,ieeechannel);
        IEEE80211_CHANCHANGE_STARTED_CLEAR(ic);
        IEEE80211_CHANCHANGE_MARKRADAR_CLEAR(ic);
    } else {
        IEEE80211_CHAN_CHANGE_UNLOCK(ic);
        qdf_print("Channel Change is already on, Please try later");
        ret = -EBUSY;
    }
    return ret;
}

QDF_STATUS wlan_pdev_wait_to_bringdown_all_vdevs(struct ieee80211com *ic)
{
    unsigned long bringdown_pend_vdev_arr[2];
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;
    uint16_t waitcnt;

    /* wait for vap stop event before letting the caller go */
    waitcnt = 0;
    while(waitcnt < OSIF_MAX_STOP_VAP_TIMEOUT_CNT) {
        /* Reset the bitmap */
        bringdown_pend_vdev_arr[0] = 0;
        bringdown_pend_vdev_arr[1] = 0;

        wlan_pdev_chan_change_pending_vdevs(pdev,
                bringdown_pend_vdev_arr,
                WLAN_MLME_SB_ID);

        /* If all the pending vdevs goes down, this would fail,
           otherwise start timer */
        if (!bringdown_pend_vdev_arr[0] && !bringdown_pend_vdev_arr[1])
            return QDF_STATUS_SUCCESS;

        schedule_timeout_interruptible(OSIF_STOP_VAP_TIMEOUT);
        waitcnt++;
    }
    if (waitcnt >= OSIF_MAX_STOP_VAP_TIMEOUT_CNT)
        qdf_err("VDEVs are not stopped bitmap %016lx : %016lx",
                bringdown_pend_vdev_arr[0],bringdown_pend_vdev_arr[1]);

    if (ic->recovery_in_progress) {
        qdf_err("FW Crash observed ...returning");
        return QDF_STATUS_E_INVAL;
    }

    return QDF_STATUS_SUCCESS;
}

int ieee80211_ucfg_set_freq_internal(wlan_if_t vap, int ieeechannel)
{
    struct ieee80211com *ic = vap->iv_ic;
    osif_dev *osnetdev = (osif_dev *)vap->iv_ifp;
    int retval;
#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
    struct wlan_objmgr_pdev *pdev = NULL;
    bool use_inter_chan = false;
#endif

    if (osnetdev->is_delete_in_progress)
        return -EINVAL;

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
    if(ic->ic_primary_allowed_enable && (ieeechannel != 0) &&
                    !(ieee80211_check_allowed_prim_chanlist(ic, ieeechannel))) {
            qdf_print("%s: channel %d is not a primary allowed channel ", __func__, ieeechannel);
            return -EINVAL;
    }
#endif

    if (ieeechannel == 0)
        ieeechannel = IEEE80211_CHAN_ANY;

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
    pdev = vap->iv_ic->ic_pdev_obj;

    if(pdev == NULL) {
        qdf_print("%s : pdev is null", __func__);
        return -EINVAL;
    }

    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
        QDF_STATUS_SUCCESS) {
        return -EINVAL;
    }
    if (ieeechannel && mlme_dfs_is_precac_enabled(pdev) &&
        (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP) &&
        ((vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80) ||
        (vap->iv_des_mode == IEEE80211_MODE_11AC_VHT160))) {
        use_inter_chan = mlme_dfs_decide_precac_preferred_chan(pdev,
                                                               (uint8_t *)&ieeechannel,
                                                               phymode2convphymode[vap->iv_des_mode]);
        /*
         * If channel change is triggered in 160MHz,
         * Change mode to 80MHz to use intermediate channel, start precac
         * on configured channel. Send OMN to notify mode change.
         */
         if (vap->iv_des_mode == IEEE80211_MODE_11AC_VHT160 &&
             use_inter_chan) {
             qdf_info("Use intermediate channel in VHT80 mode");
             wlan_set_desired_phymode(vap, IEEE80211_MODE_11AC_VHT80);
             if (vap->iv_is_up) {
                 vap->iv_chwidth = IEEE80211_CWM_WIDTH80;
                 /* Send broadcast OMN*/
                 wlan_set_param(vap, IEEE80211_OPMODE_NOTIFY_ENABLE, 1);
                 /* Send unicast OMN*/
                 wlan_iterate_station_list(vap, wlan_send_omn_action, NULL);
                 ieee80211_autochan_switch_chan_change_csa(vap->iv_ic,
                                                           ieeechannel,
                                                           IEEE80211_MODE_11AC_VHT80);
		 wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
                 return EOK;
             }
         }
     }
     wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
#endif

    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s : IBSS desired channel(%d)\n",
                __func__, ieeechannel);
        return wlan_set_desired_ibsschan(vap, ieeechannel);
    }
    else if (vap->iv_opmode == IEEE80211_M_HOSTAP || vap->iv_opmode == IEEE80211_M_MONITOR)
    {

        struct ieee80211_ath_channel *channel = NULL;
#if ATH_CHANNEL_BLOCKING
        struct ieee80211_ath_channel *tmp_channel;
#endif
        struct ieee80211vap *tmpvap = NULL;

        if(ieeechannel != IEEE80211_CHAN_ANY){
            channel = ieee80211_find_dot11_channel(ic, ieeechannel, vap->iv_des_cfreq2, vap->iv_des_mode | ic->ic_chanbwflag);
            if (channel == NULL)
            {
                channel = ieee80211_find_dot11_channel(ic, ieeechannel, 0, IEEE80211_MODE_AUTO);
                if (channel == NULL)
                    return -EINVAL;
            }

            if(ieee80211_check_chan_mode_consistency(ic,vap->iv_des_mode,channel))
            {
                struct ieee80211vap *tmpvap = NULL;

                if(IEEE80211_VAP_IS_PUREG_ENABLED(vap))
                    IEEE80211_VAP_PUREG_DISABLE(vap);
                qdf_print("Chan mode consistency failed %d %d\n setting to AUTO mode", vap->iv_des_mode,ieeechannel);

                TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                    tmpvap->iv_des_mode = IEEE80211_MODE_AUTO;
                }
            }

#if ATH_CHANNEL_BLOCKING
            tmp_channel = channel;
            channel = wlan_acs_channel_allowed(vap, channel, vap->iv_des_mode);
            if (channel == NULL)
            {
                qdf_print("channel blocked by acs");
                return -EINVAL;
            }

            if(tmp_channel != channel && ieee80211_check_chan_mode_consistency(ic,vap->iv_des_mode,channel))
            {
                qdf_print("Chan mode consistency failed %x %d %d", vap->iv_des_mode,ieeechannel,channel->ic_ieee);
                return -EINVAL;
            }
#endif

            if(IEEE80211_IS_CHAN_RADAR(channel))
            {
                qdf_print("radar detected on channel .%d",channel->ic_ieee);
                return -EINVAL;
            }

        }

        /*
         * In Monitor mode for auto channel, first valid channel is taken for configured mode.
         * In case of mutiple vaps with auto channel, AP VAP channel will be configured to monitor VAP.
         */

        if ((vap->iv_opmode == IEEE80211_M_MONITOR) && (ieeechannel == IEEE80211_CHAN_ANY)) {
            retval = wlan_set_channel(vap, ieeechannel, vap->iv_des_cfreq2);
            return retval;
        }

        if (channel != NULL) {
            if (ic->ic_curchan == channel) {
                if (vap->iv_des_chan[vap->iv_des_mode] == channel) {
                    qdf_print("\n Channel is configured already!!");
                    return EOK;
                } else if ((vap->iv_des_chan[vap->iv_des_mode] != channel) &&
                           (wlan_vdev_chan_config_valid(vap->vdev_obj) != QDF_STATUS_SUCCESS)) {
                    retval = wlan_set_channel(vap, ieeechannel, vap->iv_des_cfreq2);
                    return retval;
                }
            }
        }

        /* In case of special vap mode only one vap will be created so avoiding unnecessary delays */
        if (!vap->iv_special_vap_mode) {
            TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                if (tmpvap->iv_opmode == IEEE80211_M_STA) {
                    struct ieee80211_node *ni = tmpvap->iv_bss;
                    u_int16_t associd = ni->ni_associd;
                    IEEE80211_DELIVER_EVENT_MLME_DISASSOC_INDICATION(tmpvap, ni->ni_macaddr, associd, IEEE80211_STATUS_UNSPECIFIED);
                } else {
                    wlan_mlme_stop_vdev(tmpvap->vdev_obj, 0, WLAN_MLME_NOTIFY_NONE);
                }
            }
        }
        retval = wlan_pdev_wait_to_bringdown_all_vdevs(ic);
        if (retval == QDF_STATUS_E_INVAL)
            return -EINVAL;

        retval = wlan_set_channel(vap, ieeechannel, vap->iv_des_cfreq2);

        /* In case of special vap mode only one vap will be created so avoiding unnecessary delays */
        if (!vap->iv_special_vap_mode) {
            if(!retval) {
                TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                    struct net_device *tmpdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
                    /* Set the desired chan for other VAP to same a this VAP */
                    /* Set des chan for all VAPs to be same */
                    if(tmpvap->iv_opmode == IEEE80211_M_HOSTAP ||
                            tmpvap->iv_opmode == IEEE80211_M_MONITOR ) {
                        tmpvap->iv_des_chan[vap->iv_des_mode] =
                            vap->iv_des_chan[vap->iv_des_mode];
                    retval = (IS_UP(tmpdev) && (vap->iv_novap_reset == 0)) ? osif_vap_init(tmpdev, RESCAN) : 0;
                    }
                }
            }
        }
        return retval;
    } else {
        retval = wlan_set_channel(vap, ieeechannel, vap->iv_des_cfreq2);
        return retval;
    }
}

int ieee80211_ucfg_set_chanswitch(wlan_if_t vaphandle, u_int8_t chan, u_int8_t tbtt, u_int16_t ch_width)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int freq;
    u_int64_t flags = 0;
    struct ieee80211_ath_channel    *radar_channel = NULL;
    struct wlan_objmgr_pdev *pdev;

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s : pdev is null", __func__);
        return -EINVAL;
    }

    if (mlme_dfs_is_ap_cac_timer_running(pdev)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: CAC timer is running, doth_chanswitch is not allowed\n", __func__);
        return -EINVAL;
    }

    /* Ensure previous channel switch is not pending */
    if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: Channel change already in progress\n", __func__);
        return -EINVAL;
    }

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
    if(ic->ic_primary_allowed_enable &&
                    !ieee80211_check_allowed_prim_chanlist(ic, chan)) {
            qdf_print("%s: channel %d is not a primary allowed channel ",
                                        __func__, chan);
            return -EINVAL;
    }
#endif

    /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on channel switching
     * rules */

    if (ic->ic_strict_doth && ic->ic_non_doth_sta_cnt) {
            qdf_print("%s: strict_doth is enabled and non_doth_sta_cnt is %d"
                " Discarding channel switch request",
                __func__, ic->ic_non_doth_sta_cnt);
        return -EAGAIN;
    }

    freq = ieee80211_ieee2mhz(ic, chan, 0);
    ic->ic_chanchange_channel = NULL;

    if ((ch_width == 0) &&(ieee80211_find_channel(ic, freq, 0, ic->ic_curchan->ic_flags) == NULL)) {
        /* Switching between different modes is not allowed, print ERROR */
        qdf_print("%s(): Channel capabilities do not match, chan flags 0x%llx",
            __func__, ic->ic_curchan->ic_flags);
        return -EINVAL;
    } else {
        if(ch_width != 0){
          /* Set channel, chanflag, channel width from ch_width value */
          if (IEEE80211_IS_CHAN_11AXA(ic->ic_curchan)) {
            switch(ch_width){
            case CHWIDTH_20:
                flags = IEEE80211_CHAN_11AXA_HE20;
                break;
            case CHWIDTH_40:
                flags = IEEE80211_CHAN_11AXA_HE40PLUS;
                if (ieee80211_find_channel(ic, freq, 0, flags) == NULL) {
                    /* HE40PLUS is no good, try minus */
                    flags = IEEE80211_CHAN_11AXA_HE40MINUS;
                }
                break;
            case CHWIDTH_80:
                flags = IEEE80211_CHAN_11AXA_HE80;
                break;
            case CHWIDTH_160:
                if(IEEE80211_IS_CHAN_11AXA_HE80_80(ic->ic_curchan)){
                    flags = IEEE80211_CHAN_11AXA_HE80_80;
                }else{
                    flags = IEEE80211_CHAN_11AXA_HE160;
                }
                break;
            default:
                ch_width = CHWIDTH_20;
                flags = IEEE80211_CHAN_11AXA_HE20;
                break;
            }
          } else if (IEEE80211_IS_CHAN_VHT(ic->ic_curchan)){
            switch(ch_width){
            case CHWIDTH_20:
                flags = IEEE80211_CHAN_11AC_VHT20;
                break;
            case CHWIDTH_40:
                flags = IEEE80211_CHAN_11AC_VHT40PLUS;
                if (ieee80211_find_channel(ic, freq, 0, flags) == NULL) {
                    /*VHT40PLUS is no good, try minus*/
                    flags = IEEE80211_CHAN_11AC_VHT40MINUS;
                }
                break;
            case CHWIDTH_80:
                flags = IEEE80211_CHAN_11AC_VHT80;
                break;
            case CHWIDTH_160:
                if(IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan)){
                    flags = IEEE80211_CHAN_11AC_VHT80_80;
                }else{
                    flags = IEEE80211_CHAN_11AC_VHT160;
                }
                break;
            default:
                ch_width = CHWIDTH_20;
                flags = IEEE80211_CHAN_11AC_VHT20;
                break;
            }
          }
	  else if(IEEE80211_IS_CHAN_11N(ic->ic_curchan)){
		  switch(ch_width){
			  case CHWIDTH_20:
				  flags = IEEE80211_CHAN_11NA_HT20;
				  break;
			  case CHWIDTH_40:
				  flags = IEEE80211_CHAN_11NA_HT40PLUS;
				  if (ieee80211_find_channel(ic, freq, 0, flags) == NULL) {
					  /*HT40PLUS is no good, try minus*/
					  flags = IEEE80211_CHAN_11NA_HT40MINUS;
				  }
				  break;
			  default:
				  ch_width = CHWIDTH_20;
				  flags = IEEE80211_CHAN_11NA_HT20;
				  break;
		  }
	  }
            else{
                /*legacy doesn't support channel width change*/
                return -EINVAL;
            }
        } else {
            /* In the case of channel switch only, flags will be same as previous
             * channel */
            flags = ic->ic_curchan->ic_flags;
        }
    }

    ic->ic_chanchange_channel = ieee80211_find_channel(ic, freq, vap->iv_des_cfreq2, flags);

    if (ic->ic_chanchange_channel == NULL) {
        /* Channel is not available for the ch_width */
        return -EINVAL;
    }

    if (ic->ic_chanchange_channel == ic->ic_curchan) {
        /* If the new and old channels are the same, we are abandoning
         * the channel switch routine and returning without error. */
        qdf_err("Destination and current channels are the same. Exiting without error.");
        return 0;
    }

    /* Find destination channel width */
    ic->ic_chanchange_chwidth = ieee80211_get_chan_width(ic->ic_chanchange_channel);
    ic->ic_chanchange_chanflag  = flags;
    ic->ic_chanchange_secoffset = ieee80211_sec_chan_offset(ic->ic_chanchange_channel);

    if(ic->ic_chanchange_channel != NULL){
        radar_channel = ic->ic_chanchange_channel;
    }else{
        radar_channel = ieee80211_find_channel(ic, freq, vap->iv_des_cfreq2, ic->ic_curchan->ic_flags);
    }

    if(radar_channel){
        if(IEEE80211_IS_CHAN_RADAR(radar_channel)){
            return -EINVAL;
        }
    }else{
        return -EINVAL;
    }

    /*  flag the beacon update to include the channel switch IE */
    ic->ic_chanchange_chan = chan;
    if (tbtt) {
        ic->ic_chanchange_tbtt = tbtt;
    } else {
        ic->ic_chanchange_tbtt = IEEE80211_RADAR_11HCOUNT;
    }

    wlan_pdev_mlme_vdev_sm_csa_restart(pdev, radar_channel);

    ieee80211com_set_flags(ic, IEEE80211_F_CHANSWITCH);
    ic->ic_flags_ext2 |= IEEE80211_FEXT2_CSA_WAIT;

/* The default value of this variable is false.When NOL violation is reported
 * by FW in vap's start response, during restart of the vap, it will be reset
 * to true in dfs_action. After this if user again tries to set another NOL
 * channel using iwpriv athx doth_chanswitch "NOL_chan" "tbtt_cnt", if is
 * variable is remains to set to true, no action will be taken on vap's start
 * failure from FW. Hence resetting it here.
 */
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
    if (vap->vap_start_failure_action_taken)
        vap->vap_start_failure_action_taken = false;
#endif

    return 0;
}

wlan_chan_t ieee80211_ucfg_get_current_channel(wlan_if_t vaphandle, bool hwChan)
{
    return wlan_get_current_channel(vaphandle, hwChan);
}

wlan_chan_t ieee80211_ucfg_get_bss_channel(wlan_if_t vaphandle)
{
    return wlan_get_bss_channel(vaphandle);
}

int ieee80211_ucfg_delete_vap(wlan_if_t vap)
{
    int status = -1;
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;

    if (dev) {
        status = osif_ioctl_delete_vap(dev);
    }
    return status;
}

int ieee80211_ucfg_set_rts(wlan_if_t vap, u_int32_t val)
{
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;
    u_int32_t curval;

    curval = wlan_get_param(vap, IEEE80211_RTS_THRESHOLD);
    if (val != curval)
    {
        wlan_set_param(vap, IEEE80211_RTS_THRESHOLD, val);
        if (IS_UP(dev))
            return osif_vap_init(dev, RESCAN);
    }

    return 0;
}

int ieee80211_ucfg_set_frag(wlan_if_t vap, u_int32_t val)
{
    osif_dev *osif = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osif->netdev;
    u_int32_t curval;

    if(wlan_get_desired_phymode(vap) < IEEE80211_MODE_11NA_HT20)
    {
        curval = wlan_get_param(vap, IEEE80211_FRAG_THRESHOLD);
        if (val != curval)
        {
            wlan_set_param(vap, IEEE80211_FRAG_THRESHOLD, val);
            if (IS_UP(dev))
                return osif_vap_init(dev, RESCAN);
        }
    } else {
        qdf_print("WARNING: Fragmentation with HT mode NOT ALLOWED!!");
        return -EINVAL;
    }

    return 0;
}

int ieee80211_ucfg_set_txpow(wlan_if_t vaphandle, int txpow)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int is2GHz = IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan);
    int fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED) != 0;

    if (txpow > 0) {
        if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
            return -EINVAL;
        /*
         * txpow is in dBm while we store in 0.5dBm units
         */
        if(ic->ic_set_txPowerLimit)
            ic->ic_set_txPowerLimit(ic, 2*txpow, 2*txpow, is2GHz);
        ieee80211com_set_flags(ic, IEEE80211_F_TXPOW_FIXED);
    }
    else {
        if (!fixed) return EOK;

        if(ic->ic_set_txPowerLimit)
            ic->ic_set_txPowerLimit(ic,IEEE80211_TXPOWER_MAX,
                    IEEE80211_TXPOWER_MAX, is2GHz);
        ieee80211com_clear_flags(ic, IEEE80211_F_TXPOW_FIXED);
    }
    return EOK;
}

int ieee80211_ucfg_get_txpow(wlan_if_t vaphandle, int *txpow, int *fixed)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    *txpow = (vap->iv_bss) ? ((int16_t)vap->iv_bss->ni_txpower/2) : 0;
    *fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED) != 0;
    return 0;
}

int ieee80211_ucfg_get_txpow_fraction(wlan_if_t vaphandle, int *txpow, int *fixed)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    if (vap->iv_bss)
    qdf_print("vap->iv_bss->ni_txpower = %d", vap->iv_bss->ni_txpower);
   *txpow = (vap->iv_bss) ? ((vap->iv_bss->ni_txpower*100)/2) : 0;
   *fixed = (ic->ic_flags & IEEE80211_F_TXPOW_FIXED) != 0;
    return 0;
}


int ieee80211_ucfg_set_ap(wlan_if_t vap, u_int8_t (*des_bssid)[IEEE80211_ADDR_LEN])
{
    osif_dev *osifp = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osifp->netdev;
    u_int8_t zero_bssid[] = { 0,0,0,0,0,0 };
    int status = 0;

    if (wlan_vap_get_opmode(vap) != IEEE80211_M_STA &&
        (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_DEVICE &&
        (u_int8_t)osifp->os_opmode != IEEE80211_M_P2P_CLIENT) {
        return -EINVAL;
    }

    if (IEEE80211_ADDR_EQ(des_bssid, zero_bssid)) {
        wlan_aplist_init(vap);
    } else {
        status = wlan_aplist_set_desired_bssidlist(vap, 1, des_bssid);
    }
    if (IS_UP(dev))
        return osif_vap_init(dev, RESCAN);

    return status;
}

int ieee80211_ucfg_get_ap(wlan_if_t vap, u_int8_t *addr)
{
    int status = 0;

    static const u_int8_t zero_bssid[IEEE80211_ADDR_LEN];
    u_int8_t bssid[IEEE80211_ADDR_LEN];

    if(wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
        status = wlan_vap_get_bssid(vap, bssid);
        if(status == EOK) {
            IEEE80211_ADDR_COPY(addr, bssid);
        }
    } else {
        IEEE80211_ADDR_COPY(addr, zero_bssid);
    }

    return status;
}
extern  A_UINT32 dscp_tid_map[64];

static const struct
    {
        char *name;
        int mode;
        int elementconfig;
    } mappings[] = {
    {"AUTO",IEEE80211_MODE_AUTO,0x090909},
    {"11A",IEEE80211_MODE_11A,0x010909},
    {"11B",IEEE80211_MODE_11B,0x090909},
    {"11G",IEEE80211_MODE_11G,0x000909},
    {"FH",IEEE80211_MODE_FH,0x090909},
    {"TA",IEEE80211_MODE_TURBO_A,0x090909},
    {"TG",IEEE80211_MODE_TURBO_G,0x090909},
    {"11NAHT20",IEEE80211_MODE_11NA_HT20,0x010009},
    {"11NGHT20",IEEE80211_MODE_11NG_HT20,0x000009},
    {"11NAHT40PLUS",IEEE80211_MODE_11NA_HT40PLUS,0x010101},
    {"11NAHT40MINUS",IEEE80211_MODE_11NA_HT40MINUS,0x0101FF},
    {"11NGHT40PLUS",IEEE80211_MODE_11NG_HT40PLUS,0x000101},
    {"11NGHT40MINUS",IEEE80211_MODE_11NG_HT40MINUS,0x0001FF},
    {"11NGHT40",IEEE80211_MODE_11NG_HT40,0x000100},
    {"11NAHT40",IEEE80211_MODE_11NA_HT40,0x010100},
    {"11ACVHT20",IEEE80211_MODE_11AC_VHT20,0x010209},
    {"11ACVHT40PLUS",IEEE80211_MODE_11AC_VHT40PLUS,0x010301},
    {"11ACVHT40MINUS",IEEE80211_MODE_11AC_VHT40MINUS,0x0103FF},
    {"11ACVHT40",IEEE80211_MODE_11AC_VHT40,0x010300},
    {"11ACVHT80",IEEE80211_MODE_11AC_VHT80,0x010400},
    {"11ACVHT160",IEEE80211_MODE_11AC_VHT160,0x010500},
    {"11ACVHT80_80",IEEE80211_MODE_11AC_VHT80_80,0x010600},
    {"11AXA_HE20",IEEE80211_MODE_11AXA_HE20,0x010709},
    {"11AXG_HE20",IEEE80211_MODE_11AXG_HE20,0x000709},
    {"11AXA_HE40PLUS",IEEE80211_MODE_11AXA_HE40PLUS,0x010801},
    {"11AXA_HE40MINUS",IEEE80211_MODE_11AXA_HE40MINUS,0x0108FF},
    {"11AXG_HE40PLUS",IEEE80211_MODE_11AXG_HE40PLUS,0x000801},
    {"11AXG_HE40MINUS",IEEE80211_MODE_11AXG_HE40MINUS,0x0008FF},
    {"11AXA_HE40",IEEE80211_MODE_11AXA_HE40,0x010800},
    {"11AXG_HE40",IEEE80211_MODE_11AXG_HE40,0x000800},
    {"11AXA_HE80",IEEE80211_MODE_11AXA_HE80,0x010900},
    {"11AXA_HE160",IEEE80211_MODE_11AXA_HE160,0x010A00},
    {"11AXA_HE80_80",IEEE80211_MODE_11AXA_HE80_80,0x010B00},
};

struct elements{
#if _BYTE_ORDER == _BIG_ENDIAN
    char padd;
    char band;
    char bandwidth;
    char extchan;
#else
    char  extchan ;
    char  bandwidth;
    char  band;
    char  padd;
#endif
}  __attribute__ ((packed));

enum  {
      G = 0x0,
      A,
      B = 0x9,
};
enum  {
      NONHT =0x09,
      HT20 =0x0,
      HT40,
      VHT20,
      VHT40,
      VHT80,
      VHT160,
      VHT80_80,
      HE20,
      HE40,
      HE80,
      HE160,
      HE80_80,
};

#define INVALID_ELEMENT 0x9
#define DEFAULT_EXT_CHAN 0x0
#define MAX_SUPPORTED_MODES 33

static int ieee80211_ucfg_set_extchan( wlan_if_t vap, int extchan )
{
      int elementconfig;
      struct elements *elem;
      int i =0;
      enum ieee80211_phymode  phymode;
      phymode = wlan_get_desired_phymode(vap);
      elementconfig = mappings[phymode].elementconfig ;
      elem = (struct elements *)&elementconfig;
      elem->extchan = extchan;
      for( i = 0; i< MAX_SUPPORTED_MODES ; i ++){
          if( elementconfig == mappings[i].elementconfig)
              break;
      }
      if (i == MAX_SUPPORTED_MODES) {
          qdf_print("unsupported config ");
          return -1;
      }

      phymode=i;
      return wlan_set_desired_phymode(vap,phymode);
}

static int ieee80211_ucfg_set_bandwidth( wlan_if_t vap, int bandwidth)
{

      int elementconfig;
      struct elements *elem;
      int i =0;
      enum ieee80211_phymode  phymode;
      phymode = wlan_get_desired_phymode(vap);
      elementconfig = mappings[phymode].elementconfig ;
      elem = (struct elements *)&elementconfig;
      elem->bandwidth = bandwidth ;

      if ((bandwidth == HT20) || ( bandwidth == VHT20)){
          elem->extchan = INVALID_ELEMENT;
      }

      if (( bandwidth == HT40) || ( bandwidth == VHT40) ||  ( bandwidth == VHT80) || (bandwidth == VHT160) || (bandwidth == VHT80_80)) {
          if(elem->extchan == INVALID_ELEMENT) {
              elem->extchan = DEFAULT_EXT_CHAN;
          }
      }
      if( bandwidth == NONHT ){
          elem->extchan = INVALID_ELEMENT;
         }

      for( i = 0; i< MAX_SUPPORTED_MODES ; i ++){
          if( elementconfig == mappings[i].elementconfig)
              break;
      }
      if (i == MAX_SUPPORTED_MODES) {
          qdf_print("unsupported config ");
          return -1;
      }

      phymode=i;
      return wlan_set_desired_phymode(vap,phymode);
}

static int ieee80211_ucfg_set_band( wlan_if_t vap, int band )
{
      int elementconfig;
      struct elements *elem;
      int i =0;
      enum ieee80211_phymode  phymode;
      phymode = wlan_get_desired_phymode(vap);
      elementconfig = mappings[phymode].elementconfig ;
      elem = (struct elements *)&elementconfig;
      elem->band = band;

      if((elem->bandwidth == VHT40 || elem->bandwidth == VHT80 || elem->bandwidth == VHT160 || elem->bandwidth == VHT80_80 )&& band == G)
      {
          elem->bandwidth = HT40;
      }
      if(elem->bandwidth == VHT20 && band == G)
      {
          elem->bandwidth = HT20;
      }
      if( band == B )
      {
          elem->bandwidth = NONHT;
          elem->extchan = INVALID_ELEMENT;
      }
      for( i = 0; i< MAX_SUPPORTED_MODES ; i ++){
          if( elementconfig == mappings[i].elementconfig)
              break;
      }
      if (i == MAX_SUPPORTED_MODES) {
          qdf_print("unsupported config ");
          return -1;
      }
      phymode=i;
      return wlan_set_desired_phymode(vap,phymode);
}

int ieee80211_ucfg_get_bandwidth( wlan_if_t vap)
{
      int elementconfig;
      struct elements *elem;
      enum ieee80211_phymode  phymode;
      phymode = wlan_get_desired_phymode(vap);
      elementconfig = mappings[phymode].elementconfig ;
      elem = (struct elements *)&elementconfig;
      return(elem->bandwidth);
}
#if ATH_SUPPORT_DSCP_OVERRIDE
int ieee80211_ucfg_vap_get_dscp_tid_map(wlan_if_t vap, u_int8_t tos)
{
     if(vap->iv_dscp_map_id)
         return vap->iv_ic->ic_dscp_tid_map[vap->iv_dscp_map_id][(tos >> IP_DSCP_SHIFT) & IP_DSCP_MASK];
     else
	 return dscp_tid_map[(tos >> IP_DSCP_SHIFT) & IP_DSCP_MASK];
}

#endif
int ieee80211_ucfg_get_band( wlan_if_t vap)
{
      int elementconfig;
      struct elements *elem;
      enum ieee80211_phymode  phymode;
      phymode = wlan_get_current_phymode(vap);
      elementconfig = mappings[phymode].elementconfig ;
      elem = (struct elements *)&elementconfig;
      return(elem->band);
}

int ieee80211_ucfg_get_extchan( wlan_if_t vap)
{
      int elementconfig;
      struct elements *elem;
      enum ieee80211_phymode  phymode;
      phymode = wlan_get_desired_phymode(vap);
      elementconfig = mappings[phymode].elementconfig ;
      elem = (struct elements *)&elementconfig;
      return(elem->extchan);
}

struct find_wlan_node_req {
    wlan_node_t node;
    int assoc_id;
};

static void
find_wlan_node_by_associd(void *arg, wlan_node_t node)
{
    struct find_wlan_node_req *req = (struct find_wlan_node_req *)arg;
    if (req->assoc_id == IEEE80211_AID(wlan_node_get_associd(node))) {
        req->node = node;
    }
}

#define IEEE80211_BINTVAL_IWMAX       3500   /* max beacon interval */
#define IEEE80211_BINTVAL_IWMIN       40     /* min beacon interval */
#define IEEE80211_BINTVAL_LP_IOT_IWMIN 25    /* min beacon interval for LP IOT */
#define IEEE80211_SUBTYPE_TXPOW_SHIFT   8     /* left shift 8 bit subtype + txpower as combined value  */
#define IEEE80211_FRAMETYPE_TXPOW_SHIFT   16


int ieee80211_ucfg_set_beacon_interval(wlan_if_t vap, struct ieee80211com *ic,
        int value, bool is_vap_restart_required)
{
    int retv = 0;
    if (vap->iv_create_flags & IEEE80211_LP_IOT_VAP) {
        if (value > IEEE80211_BINTVAL_IWMAX || value < IEEE80211_BINTVAL_LP_IOT_IWMIN) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                    "BEACON_INTERVAL should be within %d to %d\n",
                    IEEE80211_BINTVAL_LP_IOT_IWMIN,
                    IEEE80211_BINTVAL_IWMAX);
            return -EINVAL;
        }
    } else if (ieee80211_vap_oce_check(vap)) {
        if (value > IEEE80211_BINTVAL_MAX || value < IEEE80211_BINTVAL_MIN) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                    "BEACON_INTERVAL should be within %d to %d\n",
                    IEEE80211_BINTVAL_MIN, IEEE80211_BINTVAL_MAX);
            return -EINVAL;
        }
    } else if (value > IEEE80211_BINTVAL_IWMAX || value < IEEE80211_BINTVAL_IWMIN) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "BEACON_INTERVAL should be within %d to %d\n",
                IEEE80211_BINTVAL_IWMIN,
                IEEE80211_BINTVAL_IWMAX);
        return -EINVAL;
    }
    retv = wlan_set_param(vap, IEEE80211_BEACON_INTVAL, value);
    if (retv == EOK) {
        wlan_if_t tmpvap;
        u_int8_t lp_vap_is_present = 0;
        u_int16_t lp_bintval = ic->ic_intval;

        /* Iterate to find if a LP IOT vap is there */
        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            if (tmpvap->iv_create_flags & IEEE80211_LP_IOT_VAP) {
                lp_vap_is_present = 1;
                /* If multiple lp iot vaps are present pick the least */
                if (lp_bintval > tmpvap->iv_bss->ni_intval)  {
                    lp_bintval = tmpvap->iv_bss->ni_intval;
                }
            }
        }

        /* Adjust regular beacon interval in ic to be a multiple of lp_iot beacon interval */
        if (lp_vap_is_present) {
            UP_CONVERT_TO_FACTOR_OF(ic->ic_intval, lp_bintval);
        }

        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            struct net_device *tmpdev = ((osif_dev *)tmpvap->iv_ifp)->netdev;
            /* Adjust regular beacon interval in ni to be a multiple of lp_iot beacon interval */
            if (lp_vap_is_present) {
                if (!(tmpvap->iv_create_flags & IEEE80211_LP_IOT_VAP)) {
                    /* up convert vap beacon interval to a factor of LP vap */
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                            "Current beacon interval %d: Checking if up conversion is needed as lp_iot vap is present. ", ic->ic_intval);
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                            "New beacon interval  %d \n", ic->ic_intval);
                    UP_CONVERT_TO_FACTOR_OF(tmpvap->iv_bss->ni_intval, lp_bintval);
                }
            }
            if (is_vap_restart_required) {
                retv = IS_UP(tmpdev) ? -osif_vap_init(tmpdev, RESCAN) : 0;
            }
        }
    }

    return retv;
}

int ieee80211_ucfg_setparam(wlan_if_t vap, int param, int value, char *extra)
{
    osif_dev  *osifp = (osif_dev *)wlan_vap_get_registered_handle(vap);
    struct net_device *dev = osifp->netdev;
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj);
    ol_txrx_vdev_handle vdev_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
    int retv = 0;
    int error = 0;
    int prev_state = 0;
    int new_state = 0;
    int *val = (int*)extra;
    int deschan;
    int basic_valid_mask = 0;
    struct _rate_table {
        int *rates;
        int nrates;
    }rate_table;
    int found = 0;
    int frame_type ;
    int frame_subtype = val[1];
    int tx_power = val[2];
    u_int8_t transmit_power = 0;
    u_int32_t loop_index;
    struct wlan_objmgr_pdev *pdev;
    QDF_STATUS status;
    uint8_t skip_restart;
    struct wlan_objmgr_vdev *vdev = vap->vdev_obj;
#ifdef WLAN_SUPPORT_FILS
    uint32_t fils_en_period = 0;
#endif /* WLAN_SUPPORT_FILS */
    uint32_t ldpc = 0;

    if (osifp->is_delete_in_progress)
        return -EINVAL;

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is null", __func__);
        return -1;
    }

    switch (param)
    {
    case IEEE80211_PARAM_PEER_TX_MU_BLACKLIST_COUNT:
        if (value <= 0) {
           qdf_print("Invalid AID value.");
           return -EINVAL;
        }
        wlan_get_mu_tx_blacklist_cnt(vap,value);
        break;
    case IEEE80211_PARAM_PEER_TX_COUNT:
        if (ic->ic_vap_set_param) {
	   retv = ic->ic_vap_set_param(vap, IEEE80211_PEER_TX_COUNT_SET, (u_int32_t)value);
        }
        break;
    case IEEE80211_PARAM_PEER_MUMIMO_TX_COUNT_RESET:
	if (ic->ic_vap_set_param) {
            retv = ic->ic_vap_set_param(vap, IEEE80211_PEER_MUMIMO_TX_COUNT_RESET_SET, (u_int32_t)value);
        }
        break;
    case IEEE80211_PARAM_PEER_POSITION:
        if (ic->ic_vap_set_param) {
	    retv = ic->ic_vap_set_param(vap, IEEE80211_PEER_POSITION_SET, (u_int32_t)value);
        }
	break;
    case IEEE80211_PARAM_SET_TXPWRADJUST:
        wlan_set_param(vap, IEEE80211_SET_TXPWRADJUST, value);
        break;
    case IEEE80211_PARAM_MAXSTA: //set max stations allowed
        if (value > ic->ic_num_clients || value < 1) { // At least one station can associate with.
            return -EINVAL;
        }
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            u_int16_t old_max_aid = vap->iv_max_aid;
            u_int16_t old_len = howmany(vap->iv_max_aid, 32) * sizeof(u_int32_t);
            if (value < vap->iv_sta_assoc) {
                qdf_print("%d station associated with vap%d! refuse this request",
                            vap->iv_sta_assoc, vap->iv_unit);
                return -EINVAL;
            }
            /* We will reject station when associated aid >= iv_max_aid, such that
            max associated station should be value + 1 */
            vap->iv_max_aid = value + 1;
            /* The interface is up, we may need to reallocation bitmap(tim, aid) */
            if (IS_UP(dev)) {
                if (vap->iv_alloc_tim_bitmap) {
                    error = vap->iv_alloc_tim_bitmap(vap);
                }
                if(!error)
                	error = wlan_node_alloc_aid_bitmap(vap, old_len);
            }
            if(!error)
                qdf_print("Setting Max Stations:%d", value);
           	else {
                qdf_print("Setting Max Stations fail");
          		vap->iv_max_aid = old_max_aid;
          		return -ENOMEM;
          	}
        }
        else {
            qdf_print("This command only support on Host AP mode.");
            return -EINVAL;
        }
        break;
    case IEEE80211_PARAM_AUTO_ASSOC:
        wlan_set_param(vap, IEEE80211_AUTO_ASSOC, value);
        break;
    case IEEE80211_PARAM_VAP_COUNTRY_IE:
        wlan_set_param(vap, IEEE80211_FEATURE_COUNTRY_IE, value);
        break;
    case IEEE80211_PARAM_VAP_DOTH:
        wlan_set_param(vap, IEEE80211_FEATURE_DOTH, value);
        break;
    case IEEE80211_PARAM_HT40_INTOLERANT:
        wlan_set_param(vap, IEEE80211_HT40_INTOLERANT, value);
        break;
    case IEEE80211_PARAM_BSS_CHAN_INFO:
        if (value < BSS_CHAN_INFO_READ || value > BSS_CHAN_INFO_READ_AND_CLEAR)
        {
            qdf_print("Setting Param value to 1(read only)");
            value = BSS_CHAN_INFO_READ;
        }
        if (ic->ic_ath_bss_chan_info_stats)
            ic->ic_ath_bss_chan_info_stats(ic, value);
        else {
            qdf_print("Not supported for DA");
            return -EINVAL;
        }
        break;

    case IEEE80211_PARAM_CHWIDTH:
        wlan_set_param(vap, IEEE80211_CHWIDTH, value);
        break;

    case IEEE80211_PARAM_CHEXTOFFSET:
        wlan_set_param(vap, IEEE80211_CHEXTOFFSET, value);
        break;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
    case IEEE80211_PARAM_STA_QUICKKICKOUT:
            wlan_set_param(vap, IEEE80211_STA_QUICKKICKOUT, value);
        break;
#endif
#if ATH_SUPPORT_DSCP_OVERRIDE
    case IEEE80211_PARAM_VAP_DSCP_PRIORITY:
        retv = wlan_set_vap_priority_dscp_tid_map(vap,value);
        if(retv == EOK)
            retv = wlan_set_param(vap, IEEE80211_VAP_DSCP_PRIORITY, value);
        break;
#endif
    case IEEE80211_PARAM_CHSCANINIT:
        wlan_set_param(vap, IEEE80211_CHSCANINIT, value);
        break;

    case IEEE80211_PARAM_COEXT_DISABLE:
        ic->ic_user_coext_disable = value;
        skip_restart = 0;
        if (value)
        {
            if (!(ic->ic_flags & IEEE80211_F_COEXT_DISABLE))
                 ieee80211com_set_flags(ic, IEEE80211_F_COEXT_DISABLE);
            else
                 skip_restart = 1;
        }
        else
        {
            if (ic->ic_flags & IEEE80211_F_COEXT_DISABLE)
                 ieee80211com_clear_flags(ic, IEEE80211_F_COEXT_DISABLE);
            else
                 skip_restart = 1;
        }
        if (!skip_restart) {
            ic->ic_need_vap_reinit = 1;
            osif_restart_for_config(ic, NULL, NULL);
        }

        break;

    case IEEE80211_PARAM_NR_SHARE_RADIO_FLAG:
#if UMAC_SUPPORT_RRM
        retv = ieee80211_set_nr_share_radio_flag(vap,value);
#endif
        break;

    case IEEE80211_PARAM_AUTHMODE:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_AUTHMODE to %s\n",
        (value == IEEE80211_AUTH_WPA) ? "WPA" : (value == IEEE80211_AUTH_8021X) ? "802.1x" :
        (value == IEEE80211_AUTH_OPEN) ? "open" : (value == IEEE80211_AUTH_SHARED) ? "shared" :
        (value == IEEE80211_AUTH_AUTO) ? "auto" : "unknown" );

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        /* Note: The PARAM_AUTHMODE will not handle WPA , that will be taken
	 * care by PARAM_WPA, so we check and update authmode for AUTO as a
	 * combination of Open and Shared , and for all others we update the value
	 * except for WPA
	 */
        if (value == IEEE80211_AUTH_AUTO) {
            error = wlan_crypto_set_vdev_param(vap->vdev_obj,
                         WLAN_CRYPTO_PARAM_AUTH_MODE,
                         (uint32_t)((1 << WLAN_CRYPTO_AUTH_OPEN) | (1 << WLAN_CRYPTO_AUTH_SHARED)));
        } else if (value != IEEE80211_AUTH_WPA) {
            error = wlan_crypto_set_vdev_param(vap->vdev_obj,
                         WLAN_CRYPTO_PARAM_AUTH_MODE,
                         (uint32_t)(1 << value));
        }

        if (error == 0) {
            if (osifp->os_opmode != IEEE80211_M_STA || !vap->iv_roam.iv_ft_enable)
                retv = ENETRESET;
        } else {
            retv = error;
        }
#else
        osifp->authmode = value;

        if (value != IEEE80211_AUTH_WPA) {
            ieee80211_auth_mode modes[1];
            u_int nmodes=1;
            modes[0] = value;
            error = wlan_set_authmodes(vap,modes,nmodes);
            if (error == 0 ) {
                if ((value == IEEE80211_AUTH_OPEN) || (value == IEEE80211_AUTH_SHARED)) {
                    error = wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 0);
                    osifp->uciphers[0] = osifp->mciphers[0] = IEEE80211_CIPHER_NONE;
                    osifp->u_count = osifp->m_count = 1;
                } else {
                    error = wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 1);
                }
            }
        }
        /*
        * set_auth_mode will reset the ucast and mcast cipher set to defaults,
        * we will reset them from our cached values for non-open mode.
        */
        if ((value != IEEE80211_AUTH_OPEN) && (value != IEEE80211_AUTH_SHARED)
                && (value != IEEE80211_AUTH_AUTO))
        {
            if (osifp->m_count)
                error = wlan_set_mcast_ciphers(vap,osifp->mciphers,osifp->m_count);
            if (osifp->u_count)
                error = wlan_set_ucast_ciphers(vap,osifp->uciphers,osifp->u_count);
        }

#ifdef ATH_SUPPORT_P2P
        /* For P2P supplicant we do not want start connnection as soon as auth mode is set */
        /* The difference in behavior between non p2p supplicant and p2p supplicant need to be fixed */
        /* see EV 73753 for more details */
        if (error == 0 && osifp->os_opmode != IEEE80211_M_P2P_CLIENT && osifp->os_opmode != IEEE80211_M_STA) {
            retv = ENETRESET;
        }
#else
        if (error == 0 ) {
            if (osifp->os_opmode != IEEE80211_M_STA || !vap->iv_roam.iv_ft_enable)
                retv = ENETRESET;
        }

#endif /* ATH_SUPPORT_P2P */
        else {
            retv = error;
        }
#endif /* WLAN_CONV_CRYPTO_SUPPORTED */
        break;
    case IEEE80211_PARAM_MCASTKEYLEN:
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_MCASTKEYLEN to %d\n", value);
        if (!(0 < value && value < IEEE80211_KEYBUF_SIZE)) {
            error = -EINVAL;
            break;
        }
        error = wlan_set_rsn_cipher_param(vap,IEEE80211_MCAST_CIPHER_LEN,value);
        retv = error;
#else
        retv = 0;
#endif
        break;
    case IEEE80211_PARAM_UCASTCIPHERS:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_UCASTCIPHERS (0x%x) %s %s %s %s %s %s %s\n",
                value, (value & 1<<IEEE80211_CIPHER_WEP) ? "WEP" : "",
                (value & 1<<IEEE80211_CIPHER_TKIP) ? "TKIP" : "",
                (value & 1<<IEEE80211_CIPHER_AES_OCB) ? "AES-OCB" : "",
                (value & 1<<IEEE80211_CIPHER_AES_CCM) ? "AES-CCMP 128" : "",
                (value & 1<<IEEE80211_CIPHER_AES_CCM_256) ? "AES-CCMP 256" : "",
                (value & 1<<IEEE80211_CIPHER_AES_GCM) ? "AES-GCMP 128" : "",
                (value & 1<<IEEE80211_CIPHER_AES_GCM_256) ? "AES-GCMP 256" : "",
                (value & 1<<IEEE80211_CIPHER_CKIP) ? "CKIP" : "",
                (value & 1<<IEEE80211_CIPHER_WAPI) ? "WAPI" : "",
                (value & 1<<IEEE80211_CIPHER_NONE) ? "NONE" : "");
        {
            int count=0;
            if (value & 1<<IEEE80211_CIPHER_WEP)
                osifp->uciphers[count++] = IEEE80211_CIPHER_WEP;
            if (value & 1<<IEEE80211_CIPHER_TKIP)
                osifp->uciphers[count++] = IEEE80211_CIPHER_TKIP;
            if (value & 1<<IEEE80211_CIPHER_AES_CCM)
                osifp->uciphers[count++] = IEEE80211_CIPHER_AES_CCM;
            if (value & 1<<IEEE80211_CIPHER_AES_CCM_256)
                osifp->uciphers[count++] = IEEE80211_CIPHER_AES_CCM_256;
            if (value & 1<<IEEE80211_CIPHER_AES_GCM)
                osifp->uciphers[count++] = IEEE80211_CIPHER_AES_GCM;
            if (value & 1<<IEEE80211_CIPHER_AES_GCM_256)
                osifp->uciphers[count++] = IEEE80211_CIPHER_AES_GCM_256;
            if (value & 1<<IEEE80211_CIPHER_CKIP)
                osifp->uciphers[count++] = IEEE80211_CIPHER_CKIP;
#if ATH_SUPPORT_WAPI
            if (value & 1<<IEEE80211_CIPHER_WAPI)
                osifp->uciphers[count++] = IEEE80211_CIPHER_WAPI;
#endif
            if (value & 1<<IEEE80211_CIPHER_NONE)
                osifp->uciphers[count++] = IEEE80211_CIPHER_NONE;
            error = wlan_set_ucast_ciphers(vap,osifp->uciphers,count);
            if (error == 0) {
                if (osifp->os_opmode != IEEE80211_M_STA || !vap->iv_roam.iv_ft_enable)
                    error = ENETRESET;
            }
            else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s Warning: wlan_set_ucast_cipher failed. cache the ucast cipher\n", __func__);
                error=0;
            }
            osifp->u_count=count;
        }
        retv = error;
        break;
    case IEEE80211_PARAM_UCASTCIPHER:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_UCASTCIPHER to %s\n",
                (value == IEEE80211_CIPHER_WEP) ? "WEP" :
                (value == IEEE80211_CIPHER_TKIP) ? "TKIP" :
                (value == IEEE80211_CIPHER_AES_OCB) ? "AES OCB" :
                (value == IEEE80211_CIPHER_AES_CCM) ? "AES CCM 128" :
                (value == IEEE80211_CIPHER_AES_CCM_256) ? "AES CCM 256" :
                (value == IEEE80211_CIPHER_AES_GCM) ? "AES GCM 128" :
                (value == IEEE80211_CIPHER_AES_GCM_256) ? "AES GCM 256" :
                (value == IEEE80211_CIPHER_CKIP) ? "CKIP" :
                (value == IEEE80211_CIPHER_WAPI) ? "WAPI" :
                (value == IEEE80211_CIPHER_NONE) ? "NONE" : "unknown");
        {
            ieee80211_cipher_type ctypes[1];
            ctypes[0] = (ieee80211_cipher_type) value;
            error = wlan_set_ucast_ciphers(vap,ctypes,1);
            /* save the ucast cipher info */
            osifp->uciphers[0] = ctypes[0];
            osifp->u_count=1;
            if (error == 0) {
                retv = ENETRESET;
            }
            else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s Warning: wlan_set_ucast_cipher failed. cache the ucast cipher\n", __func__);
                error=0;
            }
        }
        retv = error;
        break;
    case IEEE80211_PARAM_MCASTCIPHER:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_MCASTCIPHER to %s\n",
                        (value == IEEE80211_CIPHER_WEP) ? "WEP" :
                        (value == IEEE80211_CIPHER_TKIP) ? "TKIP" :
                        (value == IEEE80211_CIPHER_AES_OCB) ? "AES OCB" :
                        (value == IEEE80211_CIPHER_AES_CCM) ? "AES CCM 128" :
                        (value == IEEE80211_CIPHER_AES_CCM_256) ? "AES CCM 256" :
                        (value == IEEE80211_CIPHER_AES_GCM) ? "AES GCM 128" :
                        (value == IEEE80211_CIPHER_AES_GCM_256) ? "AES GCM 256" :
                        (value == IEEE80211_CIPHER_CKIP) ? "CKIP" :
                        (value == IEEE80211_CIPHER_WAPI) ? "WAPI" :
                        (value == IEEE80211_CIPHER_NONE) ? "NONE" : "unknown");
        {
            ieee80211_cipher_type ctypes[1];
            ctypes[0] = (ieee80211_cipher_type) value;
            error = wlan_set_mcast_ciphers(vap, ctypes, 1);
            /* save the mcast cipher info */
            osifp->mciphers[0] = ctypes[0];
            osifp->m_count=1;
            if (error) {
                /*
                * ignore the error for now.
                * both the ucast and mcast ciphers
                * are set again when auth mode is set.
                */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s", "Warning: wlan_set_mcast_cipher failed. cache the mcast cipher  \n");
                error=0;
            }
        }
        retv = error;
        break;
    case IEEE80211_PARAM_UCASTKEYLEN:
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_UCASTKEYLEN to %d\n", value);
        if (!(0 < value && value < IEEE80211_KEYBUF_SIZE)) {
            error = -EINVAL;
            break;
        }
        error = wlan_set_rsn_cipher_param(vap,IEEE80211_UCAST_CIPHER_LEN,value);
        retv = error;
#else
        retv = 0;
#endif
        break;
    case IEEE80211_PARAM_PRIVACY:
    {
        int32_t ucast_cipher;
        ucast_cipher = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
        if ( (ucast_cipher & (1<<WLAN_CRYPTO_CIPHER_NONE)) && value ) {
            wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_MCAST_CIPHER,(1 << WLAN_CRYPTO_CIPHER_WEP));
            wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_UCAST_CIPHER,(1 << WLAN_CRYPTO_CIPHER_WEP));
        }
    }
        retv = wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY,value);
        break;
    case IEEE80211_PARAM_COUNTERMEASURES:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_COUNTER_MEASURES, value);
        break;
    case IEEE80211_PARAM_HIDESSID:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_HIDE_SSID, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_APBRIDGE:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_APBRIDGE, value);
        break;
    case IEEE80211_PARAM_KEYMGTALGS:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_KEYMGTALGS (0x%x) %s %s\n",
        value, (value & WPA_ASE_8021X_UNSPEC) ? "802.1x Unspecified" : "",
        (value & WPA_ASE_8021X_PSK) ? "802.1x PSK" : "");
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
         wlan_crypto_set_vdev_param(vap->vdev_obj,
				  WLAN_CRYPTO_PARAM_KEY_MGMT,
					value);
#else
        error = wlan_set_rsn_cipher_param(vap,IEEE80211_KEYMGT_ALGS,value);
#endif
        retv = error;
        break;
    case IEEE80211_PARAM_RSNCAPS:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_RSNCAPS to 0x%x\n", value);
        if (value & RSN_CAP_MFP_ENABLED) {
            /*
             * 802.11w PMF is enabled so change hw MFP QOS bits
             */
            wlan_crypto_set_hwmfpQos(vap, 1);
        }
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	error = wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_RSN_CAP, value);
#else
        error = wlan_set_rsn_cipher_param(vap,IEEE80211_RSN_CAPS,value);
#endif
	retv = error;
        break;
    case IEEE80211_PARAM_WPA:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set IEEE80211_IOC_WPA to %s\n",
        (value == 1) ? "WPA" : (value == 2) ? "RSN" :
        (value == 3) ? "WPA and RSN" : (value == 0)? "off" : "unknown");
        if (value > 3) {
            error = -EINVAL;
            break;
        } else {
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
            ieee80211_auth_mode modes[2];
            u_int nmodes=1;
#else
	    uint32_t authmode;
#endif
            if (osifp->os_opmode == IEEE80211_M_STA ||
                osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
		error = wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_KEY_MGMT, (uint8_t)(WPA_ASE_8021X_PSK));
#else
                error = wlan_set_rsn_cipher_param(vap,IEEE80211_KEYMGT_ALGS,WPA_ASE_8021X_PSK);
#endif
                if (!error) {
                    if ((value == 3) || (value == 2)) { /* Mixed mode or WPA2 */
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                        modes[0] = IEEE80211_AUTH_RSNA;
#else
                        authmode = (1 << IEEE80211_AUTH_RSNA);
#endif
                    } else { /* WPA mode */
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                        modes[0] = IEEE80211_AUTH_WPA;
#else
                        authmode = (1 << IEEE80211_AUTH_WPA);
#endif
                    }
                }
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                /* set supported cipher to TKIP and CCM
                * to allow WPA-AES, WPA2-TKIP: and MIXED mode
                */
                osifp->u_count = 2;
                osifp->uciphers[0] = IEEE80211_CIPHER_TKIP;
                osifp->uciphers[1] = IEEE80211_CIPHER_AES_CCM;
                osifp->m_count = 2;
                osifp->mciphers[0] = IEEE80211_CIPHER_TKIP;
                osifp->mciphers[1] = IEEE80211_CIPHER_AES_CCM;
#endif
            } else {
                if (value == 3) {
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                    nmodes = 2;
                    modes[0] = IEEE80211_AUTH_WPA;
                    modes[1] = IEEE80211_AUTH_RSNA;
#else
			authmode =  (1<< IEEE80211_AUTH_WPA) | (1 << IEEE80211_AUTH_RSNA);
#endif
                } else if (value == 2) {
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                    modes[0] = IEEE80211_AUTH_RSNA;
#else
                    authmode =  (1<< IEEE80211_AUTH_RSNA);
#endif
                } else {
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                    modes[0] = IEEE80211_AUTH_WPA;
#else
                    authmode =  (1<< IEEE80211_AUTH_WPA);
#endif
                }
            }
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
            error = wlan_set_authmodes(vap,modes,nmodes);
             /*
             * set_auth_mode will reset the ucast and mcast cipher set to defaults,
             * we will reset them from our cached values.
             */
            if (osifp->m_count)
                error = wlan_set_mcast_ciphers(vap,osifp->mciphers,osifp->m_count);
            if (osifp->u_count)
                error = wlan_set_ucast_ciphers(vap,osifp->uciphers,osifp->u_count);
#else
            error = wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE, authmode);
#endif
        }
        retv = error;
        break;

    case IEEE80211_PARAM_CLR_APPOPT_IE:
        retv = wlan_set_clr_appopt_ie(vap);
        break;

    /*
    ** The setting of the manual rate table parameters and the retries are moved
    ** to here, since they really don't belong in iwconfig
    */

    case IEEE80211_PARAM_11N_RATE:
        retv = wlan_set_param(vap, IEEE80211_FIXED_RATE, value);
        break;

    case IEEE80211_PARAM_VHT_MCS:
        retv = wlan_set_param(vap, IEEE80211_FIXED_VHT_MCS, value);
    break;

    case IEEE80211_PARAM_HE_MCS:
        /* if ldpc is disabled then as per 802.11ax
         * specification, D2.0 (section 28.1.1) we
         * can not allow mcs values 10 and 11
         */
        ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_LDPC, &ldpc);
        if (ieee80211vap_ishemode(vap) &&
            (ldpc == IEEE80211_HTCAP_C_LDPC_NONE) &&
            (value >= 10))
        {
            qdf_print("MCS 10 and 11 are not allowed in \
                    HE mode if LDPC is already diabled");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_FIXED_HE_MCS, value);
    break;

    case IEEE80211_PARAM_NSS:
        if(!ic->ic_is_mode_offload(ic))
            return -EPERM;

        if (value <= 0) {
            qdf_err("Invalid value for NSS");
            return -EINVAL;
        }

        /* if ldpc is disabled then as per 802.11ax
         * specification, D2.0 (section 28.1.1) we
         * can not allow nss value > 4
         */
        ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_LDPC, &ldpc);
        if ((ldpc == IEEE80211_HTCAP_C_LDPC_NONE) &&
            (ieee80211vap_ishemode(vap) && (value > 4))) {
            qdf_print("NSS greater than 4 is not allowed in \
                    HE mode if LDPC is already diabled");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_FIXED_NSS, value);
        if (!retv) {
            wlan_vdev_mlme_set_nss(vdev, value);
        }
        /* if the novap reset is set for debugging
         * purpose we are not resetting the VAP
         */
        if ((retv == 0) && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
    break;

    case IEEE80211_PARAM_HE_UL_SHORTGI:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("UL Shortgi setting is not allowed in current mode.");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_SHORTGI, value);
        break;

    case IEEE80211_PARAM_HE_UL_LTF:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("UL LTF setting is not allowed in current mode.");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_LTF, value);
        break;

    case IEEE80211_PARAM_HE_UL_NSS:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("UL NSS setting is not allowed in current mode.");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_NSS, value);
        break;

    case IEEE80211_PARAM_HE_UL_PPDU_BW:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("UL PPDU BW setting is not allowed in current mode.");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_PPDU_BW, value);
        break;

    case IEEE80211_PARAM_HE_UL_LDPC:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("UL LDPC setting is not allowed in current mode.");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_LDPC, value);
        break;

    case IEEE80211_PARAM_HE_UL_STBC:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("UL STBC setting is not allowed in current mode.");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_STBC, value);

        break;

    case IEEE80211_PARAM_HE_UL_FIXED_RATE:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("UL MCS setting is not allowed in current mode.");
            return -EPERM;
        }

        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_FIXED_RATE, value);
        break;

    case IEEE80211_PARAM_HE_AMSDU_IN_AMPDU_SUPRT:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("HE AMSDU in AMPDU support is not allowed in current mode.");
            return -EPERM;
        }

        if (value >= 0 && value <= 1)
            vap->iv_he_amsdu_in_ampdu_suprt = value;
        else {
            qdf_err("Invalid value");
            return -EPERM;
        }
        break;

    case IEEE80211_PARAM_HE_SUBFEE_STS_SUPRT:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("HE BFEE STS support is not allowed in current mode.");
            return -EPERM;
        }

        if (!vap->iv_he_su_bfee) {
            qdf_err("HE SUBFEE_STS fields are reserved if HE SU BFEE"
                    " role is not supported");
            return -EPERM;
        }

        if ((val[1] >= 0 && val[1] <= 7) &&
            (val[2] >= 0 && val[2] <=7)) {
            vap->iv_he_subfee_sts_lteq80 = val[1];
            vap->iv_he_subfee_sts_gt80   = val[2];
        } else {
            qdf_err("Invalid value");
            return -EPERM;
        }

        qdf_info("HE SUBFEE_STS: lteq80: %d gt80: %d", val[1], val[2]);
        break;

    case IEEE80211_PARAM_HE_4XLTF_800NS_GI_RX_SUPRT:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("HE 4x LTF & 800ns GI support is not allowed in current mode.");
            return -EPERM;
        }

        if (value >= 0 && value <= 1)
            vap->iv_he_4xltf_800ns_gi = value;
        else {
            qdf_err("Invalid value");
            return -EPERM;
        }
        break;

    case IEEE80211_PARAM_HE_1XLTF_800NS_GI_RX_SUPRT:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("HE 1x LTF & 800ns GI support is not allowed in current mode.");
            return -EPERM;
        }

        if (value >= 0 && value <= 1)
            vap->iv_he_1xltf_800ns_gi = value;
        else {
            qdf_err("Invalid value");
            return -EPERM;
        }
        break;

    case IEEE80211_PARAM_HE_MAX_NC_SUPRT:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("HE Max Nc GI support is not allowed in current mode.");
            return -EPERM;
        }

        if (!vap->iv_he_su_bfee) {
            qdf_err("HE MAX_NC fields are reserved if HE SU BFEE"
                    " role is not supported");
            return -EPERM;
        }

        if (value >= 0 && value <= 7)
            vap->iv_he_max_nc = value;
        else {
            qdf_err("Invalid value");
            return -EPERM;
        }
        break;

    case IEEE80211_PARAM_TWT_RESPONDER_SUPRT:

        if(!ieee80211vap_ishemode(vap)) {
            qdf_err("HE TWT support is not allowed in current mode.");
            return -EPERM;
        }

        if (value >= 0 && value <= 1)
            vap->iv_twt_rsp = value;
        else {
            qdf_err("Invalid value");
            return -EPERM;
        }
        break;

    case IEEE80211_PARAM_NO_VAP_RESET:
        vap->iv_novap_reset = value;
    break;

    case IEEE80211_PARAM_OPMODE_NOTIFY:
        retv = wlan_set_param(vap, IEEE80211_OPMODE_NOTIFY_ENABLE, value);
    break;

    case IEEE80211_PARAM_VHT_SGIMASK:
        retv = wlan_set_param(vap, IEEE80211_VHT_SGIMASK, value);
        if (retv == 0)
            retv = ENETRESET;
    break;

    case IEEE80211_PARAM_VHT80_RATEMASK:
        retv = wlan_set_param(vap, IEEE80211_VHT80_RATEMASK, value);
        if (retv == 0)
            retv = ENETRESET;
    break;

    case IEEE80211_PARAM_BW_NSS_RATEMASK:
        retv = wlan_set_param(vap, IEEE80211_BW_NSS_RATEMASK, value);
        if (retv == 0)
            retv = ENETRESET;
    break;

    case IEEE80211_PARAM_LDPC:
        retv = wlan_set_param(vap, IEEE80211_SUPPORT_LDPC, value);
        /* if the novap reset is set for debugging
         * purpose we are not resetting the VAP
         */
        if ((retv == 0) && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
    break;

    case IEEE80211_PARAM_TX_STBC:
        retv = wlan_set_param(vap, IEEE80211_SUPPORT_TX_STBC, value);
        if (retv == 0)
            retv = ENETRESET;
    break;

    case IEEE80211_PARAM_RX_STBC:
        retv = wlan_set_param(vap, IEEE80211_SUPPORT_RX_STBC, value);
        if (retv == 0)
            retv = ENETRESET;
    break;

    case IEEE80211_PARAM_VHT_TX_MCSMAP:
        retv = wlan_set_param(vap, IEEE80211_VHT_TX_MCSMAP, value);
        if (retv == 0)
            retv = ENETRESET;
    break;

    case IEEE80211_PARAM_VHT_RX_MCSMAP:
        retv = wlan_set_param(vap, IEEE80211_VHT_RX_MCSMAP, value);
        if (retv == 0)
            retv = ENETRESET;
    break;

    case IEEE80211_PARAM_11N_RETRIES:
        if (value)
            retv = wlan_set_param(vap, IEEE80211_FIXED_RETRIES, value);
        break;
    case IEEE80211_PARAM_SHORT_GI :
        retv = wlan_set_param(vap, IEEE80211_SHORT_GI, value);
        /* if the novap reset is set for debugging
         * purpose we are not resetting the VAP
         */
        if ((retv == 0) && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_BANDWIDTH :
        retv = ieee80211_ucfg_set_bandwidth(vap, value);
        break;
    case IEEE80211_PARAM_FREQ_BAND :
         retv = ieee80211_ucfg_set_band(vap, value);
         break;

    case IEEE80211_PARAM_EXTCHAN :
         retv = ieee80211_ucfg_set_extchan(vap, value);
         break;

    case IEEE80211_PARAM_SECOND_CENTER_FREQ :
         retv = wlan_set_param(vap, IEEE80211_SECOND_CENTER_FREQ, value);
         break;

    case IEEE80211_PARAM_ATH_SUPPORT_VLAN :
         if( value == 0 || value ==1) {
             vap->vlan_set_flags = value;
             if(value == 1) {
                 dev->features &= ~NETIF_F_HW_VLAN;
             }
             if(value == 0) {
                 dev->features |= NETIF_F_HW_VLAN;
             }
         }
         break;

    case IEEE80211_DISABLE_BCN_BW_NSS_MAP :
         if(value >= 0) {
             ic->ic_disable_bcn_bwnss_map = (value ? 1: 0);
             retv = EOK;
         }
         else
             retv = EINVAL;
         break;

    case IEEE80211_DISABLE_STA_BWNSS_ADV:
         if (value >= 0) {
             ic->ic_disable_bwnss_adv = (value ? 1: 0);
	     retv = EOK;
         } else
             retv = EINVAL;
	 break;
#if DBG_LVL_MAC_FILTERING
    case IEEE80211_PARAM_DBG_LVL_MAC:
          /* This takes 8 bytes as arguments <set/clear> <mac addr> <enable/disable>
          *  e.g. dbgLVLmac 1 0xaa 0xbb 0xcc 0xdd 0xee 0xff 1
          */
         retv = wlan_set_debug_mac_filtering_flags(vap, (unsigned char *)extra);
         break;
#endif

    case IEEE80211_PARAM_UMAC_VERBOSE_LVL:
        if (((value >= IEEE80211_VERBOSE_FORCE) && (value <= IEEE80211_VERBOSE_TRACE)) || (value == IEEE80211_VERBOSE_OFF)){
            wlan_set_umac_verbose_level(value);
        } else
            retv = EINVAL;
        break;

    case IEEE80211_PARAM_CONFIG_CATEGORY_VERBOSE:
         retv = wlan_set_shared_print_ctrl_category_verbose(value);
         break;

    case IEEE80211_PARAM_SET_VLAN_TYPE:
         ieee80211_ucfg_set_vlan_type(osifp, val[1], val[2]);
         break;

    case IEEE80211_PARAM_LOG_FLUSH_TIMER_PERIOD:
         retv = wlan_set_qdf_flush_timer_period(value);
         break;

    case IEEE80211_PARAM_LOG_FLUSH_ONE_TIME:
         wlan_set_qdf_flush_logs();
         break;

    case IEEE80211_PARAM_LOG_DUMP_AT_KERNEL_ENABLE:
         wlan_set_log_dump_at_kernel_level(value);
         break;

#ifdef QCA_OL_DMS_WAR
    case IEEE80211_PARAM_DMS_AMSDU_WAR:
         if(value < 0 || value > 1)
         {
             qdf_print("INVALID value. Please use 1 to enable and 0 to disable");
             break;
         }
         vap->dms_amsdu_war = value;
         /* Reset pkt_type set in CDP vdev->hdr_cache */
         if (!value)
             wlan_set_param(vap, IEEE80211_VAP_TX_ENCAP_TYPE, htt_cmn_pkt_type_ethernet);
         break;
#endif

    case IEEE80211_PARAM_DBG_LVL:
         /*
          * NB: since the value is size of integer, we could only set the 32
          * LSBs of debug mask
          */
         if (vap->iv_csl_support && LOG_CSL_BASIC) {
             value |= IEEE80211_MSG_CSL;
         }
         {
             u_int64_t old_val = wlan_get_debug_flags(vap);
             retv = wlan_set_debug_flags(vap,(old_val & 0xffffffff00000000) | (u_int32_t)(value));
         }
         break;

    case IEEE80211_PARAM_DBG_LVL_HIGH:
        /*
         * NB: This sets the upper 32 LSBs
         */
        {
            u_int64_t old = wlan_get_debug_flags(vap);
            retv = wlan_set_debug_flags(vap, (old & 0xffffffff) | ((u_int64_t) value << 32));
        }
        break;
#if UMAC_SUPPORT_IBSS
    case IEEE80211_PARAM_IBSS_CREATE_DISABLE:
        if (osifp->os_opmode != IEEE80211_M_IBSS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "Can not be used in mode %d\n", osifp->os_opmode);
            return -EINVAL;
        }
        osifp->disable_ibss_create = !!value;
        break;
#endif
	case IEEE80211_PARAM_WEATHER_RADAR_CHANNEL:
        retv = wlan_set_param(vap, IEEE80211_WEATHER_RADAR, value);
        /* Making it zero so that it gets updated in Beacon */
        if ( EOK == retv)
            vap->iv_country_ie_chanflags = 0;
		break;
    case IEEE80211_PARAM_SEND_DEAUTH:
        retv = wlan_set_param(vap,IEEE80211_SEND_DEAUTH,value);
        break;
    case IEEE80211_PARAM_WEP_KEYCACHE:
        retv = wlan_set_param(vap, IEEE80211_WEP_KEYCACHE, value);
        break;
    case IEEE80211_PARAM_SIFS_TRIGGER:
        if (ic->ic_vap_sifs_trigger) {
            retv = ic->ic_vap_sifs_trigger(vap, value);
        }
        break;
    case IEEE80211_PARAM_BEACON_INTERVAL:
        retv = ieee80211_ucfg_set_beacon_interval(vap, ic, value, 1);
        break;
#if ATH_SUPPORT_AP_WDS_COMBO
    case IEEE80211_PARAM_NO_BEACON:
        retv = wlan_set_param(vap, IEEE80211_NO_BEACON, value);
        break;
#endif
    case IEEE80211_PARAM_PUREG:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_PUREG, value);
        /* NB: reset only if we're operating on an 11g channel */
        if (retv == 0) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC &&
                (IEEE80211_IS_CHAN_ANYG(chan) ||
                IEEE80211_IS_CHAN_11NG(chan)))
                retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_PUREN:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_PURE11N, value);
        /* Reset only if we're operating on a 11ng channel */
        if (retv == 0) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC &&
            IEEE80211_IS_CHAN_11NG(chan))
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_PURE11AC:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_PURE11AC, value);
        /* Reset if the channel is valid */
        if (retv == EOK) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC) {
                retv = ENETRESET;
	        }
        }
        break;
    case IEEE80211_PARAM_STRICT_BW:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_STRICT_BW, value);
        /* Reset if the channel is valid */
        if (retv == EOK) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC) {
                retv = ENETRESET;
	        }
        }
        break;
    case IEEE80211_PARAM_WDS:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_WDS, value);
        if (retv == 0) {
            /* WAR: set the auto assoc feature also for WDS */
            if (value) {
                wlan_set_param(vap, IEEE80211_AUTO_ASSOC, 1);
                /* disable STA powersave for WDS */
                if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
                    (void) wlan_set_powersave(vap,IEEE80211_PWRSAVE_NONE);
                    (void) wlan_pwrsave_force_sleep(vap,0);
                }
            }
        }
        break;
    case IEEE80211_PARAM_DA_WAR_ENABLE:
        if ((ic->ic_get_tgt_type(ic) == TARGET_TYPE_QCA8074) &&
           (wlan_vap_get_opmode(vap) == IEEE80211_M_HOSTAP)) {
            cdp_txrx_set_vdev_param(wlan_psoc_get_dp_handle(psoc),
            (struct cdp_vdev *)vdev_txrx_handle, CDP_ENABLE_DA_WAR,
            value);
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "Valid only in HKv1 AP mode\n");
            retv = EINVAL;
        }
        break;
#if WDS_VENDOR_EXTENSION
    case IEEE80211_PARAM_WDS_RX_POLICY:
        retv = wlan_set_param(vap, IEEE80211_WDS_RX_POLICY, value);
        break;
#endif
    case IEEE80211_PARAM_VAP_PAUSE_SCAN:
        if (ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic)) {
            vap->iv_pause_scan = value ;
            retv = 0;
        } else retv =  EINVAL;
        break;
#if ATH_GEN_RANDOMNESS
    case IEEE80211_PARAM_RANDOMGEN_MODE:
        if(value < 0 || value > 2)
        {
         qdf_print("INVALID mode please use between modes 0 to 2");
         break;
        }
        ic->random_gen_mode = value;
        break;
#endif
    case IEEE80211_PARAM_VAP_ENHIND:
        if (value) {
            retv = wlan_set_param(vap, IEEE80211_FEATURE_VAP_ENHIND, value);
        }
        else {
            retv = wlan_set_param(vap, IEEE80211_FEATURE_VAP_ENHIND, value);
        }
        break;

    case IEEE80211_PARAM_BLOCKDFSCHAN:
    {
        if (value)
        {
            ieee80211com_set_flags_ext(ic, IEEE80211_FEXT_BLKDFSCHAN);
        }
        else
        {
            ieee80211com_clear_flags_ext(ic, IEEE80211_FEXT_BLKDFSCHAN);
        }

        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_BLKDFSCHAN, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
    }
        break;
#if ATH_SUPPORT_WAPI
    case IEEE80211_PARAM_SETWAPI:
        retv = wlan_setup_wapi(vap, value);
        if (retv == 0) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_WAPIREKEY_USK:
        retv = wlan_set_wapirekey_unicast(vap, value);
        break;
    case IEEE80211_PARAM_WAPIREKEY_MSK:
        retv = wlan_set_wapirekey_multicast(vap, value);
        break;
    case IEEE80211_PARAM_WAPIREKEY_UPDATE:
        retv = wlan_set_wapirekey_update(vap, (unsigned char*)&extra[4]);
        break;
#endif
#if WLAN_SUPPORT_GREEN_AP
    case IEEE80211_IOCTL_GREEN_AP_PS_ENABLE:
        ucfg_green_ap_config(pdev, value ? 1 : 0);
        retv = 0;
        break;

    case IEEE80211_IOCTL_GREEN_AP_PS_TIMEOUT:
        ucfg_green_ap_set_transition_time(pdev, ((value > 20) && (value < 0xFFFF)) ? value : 20);
        retv = 0;
        break;

    case IEEE80211_IOCTL_GREEN_AP_ENABLE_PRINT:
        ucfg_green_ap_enable_debug_prints(pdev, value?1:0);
        break;
#endif
    case IEEE80211_PARAM_WPS:
        retv = wlan_set_param(vap, IEEE80211_WPS_MODE, value);
        break;
    case IEEE80211_PARAM_EXTAP:
        if (value) {
            if (value == 3 /* dbg */) {
                if (ic->ic_miroot)
                    mi_tbl_dump(ic->ic_miroot);
                else
                    dp_extap_mitbl_dump(dp_get_extap_handle(vdev));
                break;
            }
            if (value == 2 /* dbg */) {
                dp_extap_disable(vdev);
                if (ic->ic_miroot)
                    mi_tbl_purge(&ic->ic_miroot);
                else
                    dp_extap_mitbl_purge(dp_get_extap_handle(vdev));
            }
            dp_extap_enable(vdev);
            /* Set the auto assoc feature for Extender Station */
            wlan_set_param(vap, IEEE80211_AUTO_ASSOC, 1);
            wlan_set_param(vap, IEEE80211_FEATURE_EXTAP, 1);
            if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
                (void) wlan_set_powersave(vap,IEEE80211_PWRSAVE_NONE);
                (void) wlan_pwrsave_force_sleep(vap,0);
                /* Enable enhanced independent repeater mode for EXTAP */
                retv = wlan_set_param(vap, IEEE80211_FEATURE_VAP_ENHIND, value);
            }
        } else {
            dp_extap_disable(vdev);
            wlan_set_param(vap, IEEE80211_FEATURE_EXTAP, 0);
            if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
                retv = wlan_set_param(vap, IEEE80211_FEATURE_VAP_ENHIND, value);
            }
        }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_vops)
        ic->nss_vops->ic_osif_nss_vdev_set_cfg(osifp, OSIF_NSS_VDEV_EXTAP_CONFIG);
#endif
        break;
    case IEEE80211_PARAM_STA_FORWARD:
    retv = wlan_set_param(vap, IEEE80211_FEATURE_STAFWD, value);
    break;

    case IEEE80211_PARAM_DYN_BW_RTS:
        retv = ic->ic_vap_dyn_bw_rts(vap, value);
        if (retv == EOK) {
            vap->dyn_bw_rts = value;
        } else {
            return -EINVAL;
        }
        break;

    case IEEE80211_PARAM_CWM_EXTPROTMODE:
        if (value >= 0) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_EXTPROTMODE, value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        } else {
            retv = -EINVAL;
        }
        break;
    case IEEE80211_PARAM_CWM_EXTPROTSPACING:
        if (value >= 0) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_EXTPROTSPACING, value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        }
        else {
            retv = -EINVAL;
        }
        break;
    case IEEE80211_PARAM_CWM_ENABLE:
        if (value >= 0) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_ENABLE, value);
            if ((retv == EOK) && (vap->iv_novap_reset == 0)) {
                retv = ENETRESET;
            }
        } else {
            retv = -EINVAL;
        }
        break;
    case IEEE80211_PARAM_CWM_EXTBUSYTHRESHOLD:
        if (value >=0 && value <=100) {
            retv = wlan_set_device_param(ic,IEEE80211_DEVICE_CWM_EXTBUSYTHRESHOLD, value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        } else {
            retv = -EINVAL;
        }
        break;
    case IEEE80211_PARAM_DOTH:
        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_DOTH, value);
        if (retv == EOK) {
            retv = ENETRESET;   /* XXX: need something this drastic? */
        }
        break;
    case IEEE80211_PARAM_SETADDBAOPER:
        if (value > 1 || value < 0) {
            return -EINVAL;
        }

        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_ADDBA_MODE, value);
        if((!retv) && (ic->ic_vap_set_param)) {
            retv = ic->ic_vap_set_param(vap, IEEE80211_CONFIG_ADDBA_MODE,
                                        value);
        }
        break;
    case IEEE80211_PARAM_PROTMODE:
        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_PROTECTION_MODE, value);
        /* NB: if not operating in 11g this can wait */
        if (retv == EOK) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC &&
                (IEEE80211_IS_CHAN_ANYG(chan) ||
                IEEE80211_IS_CHAN_11NG(chan))) {
                retv = ENETRESET;
            }
        }
        break;
    case IEEE80211_PARAM_ROAMING:
        if (!(IEEE80211_ROAMING_DEVICE <= value &&
            value <= IEEE80211_ROAMING_MANUAL))
            return -EINVAL;
        ic->ic_roaming = value;
        if(value == IEEE80211_ROAMING_MANUAL)
            IEEE80211_VAP_AUTOASSOC_DISABLE(vap);
        else
            IEEE80211_VAP_AUTOASSOC_ENABLE(vap);
        break;
    case IEEE80211_PARAM_DROPUNENCRYPTED:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_DROP_UNENC, value);
        break;
    case IEEE80211_PARAM_DRIVER_CAPS:
        retv = wlan_set_param(vap, IEEE80211_DRIVER_CAPS, value); /* NB: for testing */
        break;
    case IEEE80211_PARAM_STA_MAX_CH_CAP:
        /* This flag will be enabled only on a STA VAP */
        if (vap->iv_opmode == IEEE80211_M_STA) {
            vap->iv_sta_max_ch_cap = !!value;
        } else {
            qdf_err("Config is for STA VAP only");
            return -EINVAL;
        }
        break;
    case IEEE80211_PARAM_OBSS_NB_RU_TOLERANCE_TIME:
        if ((value >= IEEE80211_OBSS_NB_RU_TOLERANCE_TIME_MIN) &&
            (value <= IEEE80211_OBSS_NB_RU_TOLERANCE_TIME_MAX)) {
            ic->ic_obss_nb_ru_tolerance_time = value;
        } else {
            qdf_err("Invalid value for NB RU tolerance time\n");
            return -EINVAL;
        }
        break;
/*
* Support for Mcast Enhancement
*/
#if ATH_SUPPORT_IQUE
    case IEEE80211_PARAM_ME:
        wlan_set_param(vap, IEEE80211_ME, value);
        break;
#if ATH_SUPPORT_ME_SNOOP_TABLE
    case IEEE80211_PARAM_MEDEBUG:
        wlan_set_param(vap, IEEE80211_MEDEBUG, value);
        break;
    case IEEE80211_PARAM_ME_SNOOPLENGTH:
        wlan_set_param(vap, IEEE80211_ME_SNOOPLENGTH, value);
        break;
    case IEEE80211_PARAM_ME_TIMEOUT:
        wlan_set_param(vap, IEEE80211_ME_TIMEOUT, value);
        break;
    case IEEE80211_PARAM_ME_DROPMCAST:
        wlan_set_param(vap, IEEE80211_ME_DROPMCAST, value);
        break;
    case IEEE80211_PARAM_ME_CLEARDENY:
        wlan_set_param(vap, IEEE80211_ME_CLEARDENY, value);
        break;
#endif
    case IEEE80211_PARAM_HBR_TIMER:
        wlan_set_param(vap, IEEE80211_HBR_TIMER, value);
        break;
#endif

    case IEEE80211_PARAM_SCANVALID:
        if (osifp->os_opmode == IEEE80211_M_STA ||
                osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
            if (wlan_connection_sm_set_param(osifp->sm_handle,
                                             WLAN_CONNECTION_PARAM_SCAN_CACHE_VALID_TIME, value) == -EINVAL) {
                retv = -EINVAL;
            }
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "Can not be used in mode %d\n", osifp->os_opmode);
            retv = -EINVAL;
        }
        break;

    case IEEE80211_PARAM_DTIM_PERIOD:
        if (!(osifp->os_opmode == IEEE80211_M_HOSTAP ||
            osifp->os_opmode == IEEE80211_M_IBSS)) {
            return -EINVAL;
        }
        if (value > IEEE80211_DTIM_MAX ||
            value < IEEE80211_DTIM_MIN) {

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                              "DTIM_PERIOD should be within %d to %d\n",
                              IEEE80211_DTIM_MIN,
                              IEEE80211_DTIM_MAX);
            return -EINVAL;
        }
        retv = wlan_set_param(vap, IEEE80211_DTIM_INTVAL, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }

        break;
    case IEEE80211_PARAM_MACCMD:
        wlan_set_acl_policy(vap, value, IEEE80211_ACL_FLAG_ACL_LIST_1);
        break;
    case IEEE80211_PARAM_ENABLE_OL_STATS:
        /* This param should be eventually removed and re-used */
        qdf_print("Issue this command on parent device, like wifiX");
        break;
    case IEEE80211_PARAM_RTT_ENABLE:
        qdf_print("KERN_DEBUG\n setting the rtt enable flag");
        vap->rtt_enable = value;
        break;
    case IEEE80211_PARAM_LCI_ENABLE:
        qdf_print("KERN_DEBUG\n setting the lci enble flag");
        vap->lci_enable = value;
        break;
    case IEEE80211_PARAM_LCR_ENABLE:
        qdf_print("KERN_DEBUG\n setting the lcr enble flag");
        vap->lcr_enable = value;
        break;
    case IEEE80211_PARAM_MCAST_RATE:
        /*
        * value is rate in units of Kbps
        * min: 1Mbps max: 350Mbps
        */
        if (value < ONEMBPS || value > THREE_HUNDRED_FIFTY_MBPS)
            retv = -EINVAL;
        else {
            retv = wlan_set_param(vap, IEEE80211_MCAST_RATE, value);
        }
        break;
    case IEEE80211_PARAM_BCAST_RATE:
        /*
        * value is rate in units of Kbps
        * min: 1Mbps max: 350Mbps
        */
        if (value < ONEMBPS || value > THREE_HUNDRED_FIFTY_MBPS)
            retv = -EINVAL;
        else {
        	retv = wlan_set_param(vap, IEEE80211_BCAST_RATE, value);
        }
        break;
    case IEEE80211_PARAM_MGMT_RATE:
        if(!ieee80211_rate_is_valid_basic(vap,value)){
            qdf_print("%s: rate %d is not valid. ",__func__,value);
            retv = -EINVAL;
            break;
        }
       /*
        * value is rate in units of Kbps
        * min: 1000 kbps max: 300000 kbps
        */
        if (value < 1000 || value > 300000)
            retv = -EINVAL;
        else {
            retv = wlan_set_param(vap, IEEE80211_MGMT_RATE, value);
            /* Set beacon rate through separate vdev param */
            retv = wlan_set_param(vap, IEEE80211_BEACON_RATE_FOR_VAP, value);
        }
        break;
    case IEEE80211_PARAM_PRB_RATE:
        if(!ieee80211_rate_is_valid_basic(vap,value)){
            qdf_print("%s: rate %d is not valid. ", __func__, value);
            retv = -EINVAL;
            break;
        }
       /*
        * value is rate in units of Kbps
        * min: 1000 kbps max: 300000 kbps
        */
        if (value < 1000 || value > 300000)
            retv = -EINVAL;
        else {
            retv = wlan_set_param(vap, IEEE80211_PRB_RATE, value);
        }
        break;
    case IEEE80211_RTSCTS_RATE:
        if (!ieee80211_rate_is_valid_basic(vap,value)) {
            qdf_print("%s: Rate %d is not valid. ",__func__,value);
            retv = -EINVAL;
            break;
        }
       /*
        * Here value represents rate in Kbps.
        * min: 1000 kbps max: 24000 kbps
        */
        if (value < ONEMBPS || value > HIGHEST_BASIC_RATE)
            retv = -EINVAL;
        else {
            retv = wlan_set_param(vap, IEEE80211_RTSCTS_RATE, value);
        }
        break;
    case IEEE80211_PARAM_CCMPSW_ENCDEC:
        if (value) {
            IEEE80211_VAP_CCMPSW_ENCDEC_ENABLE(vap);
        } else {
            IEEE80211_VAP_CCMPSW_ENCDEC_DISABLE(vap);
        }
        break;
    case IEEE80211_PARAM_NETWORK_SLEEP:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s set IEEE80211_IOC_POWERSAVE parameter %d \n",
                          __func__,value );
        do {
            ieee80211_pwrsave_mode ps_mode = IEEE80211_PWRSAVE_NONE;
            switch(value) {
            case 0:
                ps_mode = IEEE80211_PWRSAVE_NONE;
                break;
            case 1:
                ps_mode = IEEE80211_PWRSAVE_LOW;
                break;
            case 2:
                ps_mode = IEEE80211_PWRSAVE_NORMAL;
                break;
            case 3:
                ps_mode = IEEE80211_PWRSAVE_MAXIMUM;
                break;
            }
            error= wlan_set_powersave(vap,ps_mode);
        } while(0);
        break;

#if UMAC_SUPPORT_WNM
    case IEEE80211_PARAM_WNM_SLEEP:
        if (wlan_wnm_vap_is_set(vap) && ieee80211_wnm_sleep_is_set(vap->wnm)) {
            ieee80211_pwrsave_mode ps_mode = IEEE80211_PWRSAVE_NONE;
            if (value > 0)
                ps_mode = IEEE80211_PWRSAVE_WNM;
            else
                ps_mode = IEEE80211_PWRSAVE_NONE;

            if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA)
                vap->iv_wnmsleep_intval = value > 0 ? value : 0;
            error = wlan_set_powersave(vap,ps_mode);
            qdf_print("set IEEE80211_PARAM_WNM_SLEEP mode = %d", ps_mode);
        } else
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s: WNM not supported\n", __func__);
	break;

    case IEEE80211_PARAM_WNM_SMENTER:
        if (!wlan_wnm_vap_is_set(vap) || !ieee80211_wnm_sleep_is_set(vap->wnm)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s: WNM not supported\n", __func__);
            return -EINVAL;
        }

        if (value % 2 == 0) {
            /* HACK: even interval means FORCE WNM Sleep: requires manual wnmsmexit */
            vap->iv_wnmsleep_force = 1;
        }

        ieee80211_wnm_sleepreq_to_app(vap, IEEE80211_WNMSLEEP_ACTION_ENTER, value);
        break;

    case IEEE80211_PARAM_WNM_SMEXIT:
        if (!wlan_wnm_vap_is_set(vap) || !ieee80211_wnm_sleep_is_set(vap->wnm)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s: WNM not supported\n", __func__);
            return -EINVAL;
        }
        vap->iv_wnmsleep_force = 0;
        ieee80211_wnm_sleepreq_to_app(vap, IEEE80211_WNMSLEEP_ACTION_EXIT, value);
	    break;
#endif

#ifdef ATHEROS_LINUX_PERIODIC_SCAN
    case IEEE80211_PARAM_PERIODIC_SCAN:
        if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
            if (osifp->os_periodic_scan_period != value){
                if (value && (value < OSIF_PERIODICSCAN_MIN_PERIOD))
                    osifp->os_periodic_scan_period = OSIF_PERIODICSCAN_MIN_PERIOD;
                else
                    osifp->os_periodic_scan_period = value;

                retv = ENETRESET;
            }
        }
        break;
#endif

#if ATH_SW_WOW
    case IEEE80211_PARAM_SW_WOW:
        if (wlan_vap_get_opmode(vap) == IEEE80211_M_STA) {
            retv = wlan_set_wow(vap, value);
        }
        break;
#endif

    case IEEE80211_PARAM_UAPSDINFO:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_UAPSD, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
	break ;
#if defined(UMAC_SUPPORT_STA_POWERSAVE) || defined(ATH_PERF_PWR_OFFLOAD)
    /* WFD Sigma use these two to do reset and some cases. */
    case IEEE80211_PARAM_SLEEP:
        /* XXX: Forced sleep for testing. Does not actually place the
         *      HW in sleep mode yet. this only makes sense for STAs.
         */
        /* enable/disable force  sleep */
        wlan_pwrsave_force_sleep(vap,value);
        break;
#endif
     case IEEE80211_PARAM_COUNTRY_IE:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_IC_COUNTRY_IE, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_2G_CSA:
        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_2G_CSA, value);
        break;
#if UMAC_SUPPORT_QBSSLOAD
    case IEEE80211_PARAM_QBSS_LOAD:
        if (value > 1 || value < 0) {
            return -EINVAL;
        } else {
            retv = wlan_set_param(vap, IEEE80211_QBSS_LOAD, value);
            if (retv == EOK)
                retv = ENETRESET;
        }
        break;
#if ATH_SUPPORT_HS20
    case IEEE80211_PARAM_HC_BSSLOAD:
        retv = wlan_set_param(vap, IEEE80211_HC_BSSLOAD, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_OSEN:
        if (value > 1 || value < 0)
            return -EINVAL;
        else
            wlan_set_param(vap, IEEE80211_OSEN, value);
        break;
#endif /* ATH_SUPPORT_HS20 */
#endif /* UMAC_SUPPORT_QBSSLOAD */
#if UMAC_SUPPORT_XBSSLOAD
    case IEEE80211_PARAM_XBSS_LOAD:
        if (value > 1 || value < 0) {
            return -EINVAL;
        } else {
            retv = wlan_set_param(vap, IEEE80211_XBSS_LOAD, value);
            if (retv == EOK)
                retv = ENETRESET;
        }
        break;
#endif
#if UMAC_SUPPORT_CHANUTIL_MEASUREMENT
    case IEEE80211_PARAM_CHAN_UTIL_ENAB:
        if (value > 1 || value < 0) {
            return -EINVAL;
        } else {
            retv = wlan_set_param(vap, IEEE80211_CHAN_UTIL_ENAB, value);
            if (retv == EOK)
                retv = ENETRESET;
        }
        break;
#endif /* UMAC_SUPPORT_CHANUTIL_MEASUREMENT */
#if UMAC_SUPPORT_QUIET
    case IEEE80211_PARAM_QUIET_PERIOD:
        if (vap->iv_bcn_offload_enable) {
            if (value > MAX_QUIET_ENABLE_FLAG || value < 0) {
                return -EINVAL;
            } else {
                retv = wlan_quiet_set_param(vap, value);
            }
        } else {
            if (value > 1 || value < 0) {
                return -EINVAL;
            } else {
                retv = wlan_quiet_set_param(vap, value);
                if (retv == EOK)
                    retv = ENETRESET;
            }
        }
        break;
#endif /* UMAC_SUPPORT_QUIET */
    case IEEE80211_PARAM_START_ACS_REPORT:
        retv = wlan_set_param(vap, IEEE80211_START_ACS_REPORT, !!value);
        break;
    case IEEE80211_PARAM_MIN_DWELL_ACS_REPORT:
        retv = wlan_set_param(vap, IEEE80211_MIN_DWELL_ACS_REPORT, value);
        break;
    case IEEE80211_PARAM_MAX_DWELL_ACS_REPORT:
        retv = wlan_set_param(vap, IEEE80211_MAX_DWELL_ACS_REPORT, value);
        break;
    case IEEE80211_PARAM_SCAN_MIN_DWELL:
        retv = wlan_set_param(vap, IEEE80211_SCAN_MIN_DWELL, value);
       break;
    case IEEE80211_PARAM_SCAN_MAX_DWELL:
        retv = wlan_set_param(vap, IEEE80211_SCAN_MAX_DWELL, value);
       break;
    case IEEE80211_PARAM_MAX_SCAN_TIME_ACS_REPORT:
        retv = wlan_set_param(vap, IEEE80211_MAX_SCAN_TIME_ACS_REPORT, value);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_LONG_DUR:
        retv = wlan_set_param(vap,IEEE80211_ACS_CH_HOP_LONG_DUR, value);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_NO_HOP_DUR:
        retv = wlan_set_param(vap,IEEE80211_ACS_CH_HOP_NO_HOP_DUR,value);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_CNT_WIN_DUR:
        retv = wlan_set_param(vap,IEEE80211_ACS_CH_HOP_CNT_WIN_DUR, value);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_NOISE_TH:
        retv = wlan_set_param(vap,IEEE80211_ACS_CH_HOP_NOISE_TH,value);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_CNT_TH:
        retv = wlan_set_param(vap,IEEE80211_ACS_CH_HOP_CNT_TH, value);
        break;
    case IEEE80211_PARAM_ACS_ENABLE_CH_HOP:
        retv = wlan_set_param(vap,IEEE80211_ACS_ENABLE_CH_HOP, value);
        break;
    case IEEE80211_PARAM_MBO:
        retv = wlan_set_param(vap, IEEE80211_MBO, !!value);
        if (retv == EOK)
            retv = ENETRESET;
        break;
    case IEEE80211_PARAM_MBO_ASSOC_DISALLOW:
        retv = wlan_set_param(vap, IEEE80211_MBO_ASSOC_DISALLOW,value);
        break;
    case IEEE80211_PARAM_MBO_CELLULAR_PREFERENCE:
        retv = wlan_set_param(vap,IEEE80211_MBO_CELLULAR_PREFERENCE,value);
        break;
    case IEEE80211_PARAM_MBO_TRANSITION_REASON:
        retv  = wlan_set_param(vap,IEEE80211_MBO_TRANSITION_REASON,value);
        break;
    case IEEE80211_PARAM_MBO_ASSOC_RETRY_DELAY:
        retv  = wlan_set_param(vap,IEEE80211_MBO_ASSOC_RETRY_DELAY,value);
        break;
    case IEEE80211_PARAM_MBO_CAP:
        retv = wlan_set_param(vap, IEEE80211_MBOCAP, value);
        if (retv == EOK)
            retv = ENETRESET;
        break;
    case IEEE80211_PARAM_OCE:
        retv = wlan_set_param(vap, IEEE80211_OCE, !!value);
        if (retv == EOK)
            retv = ENETRESET;
        break;
    case IEEE80211_PARAM_OCE_ASSOC_REJECT:
        retv = wlan_set_param(vap, IEEE80211_OCE_ASSOC_REJECT, !!value);
        break;
    case IEEE80211_PARAM_OCE_ASSOC_MIN_RSSI:
        retv = wlan_set_param(vap, IEEE80211_OCE_ASSOC_MIN_RSSI, value);
        break;
    case IEEE80211_PARAM_OCE_ASSOC_RETRY_DELAY:
        retv = wlan_set_param(vap, IEEE80211_OCE_ASSOC_RETRY_DELAY, value);
        break;
    case IEEE80211_PARAM_OCE_WAN_METRICS:
        retv = wlan_set_param(vap, IEEE80211_OCE_WAN_METRICS, ((val[1] & 0xFFFF) << 16) | (val[2] & 0xFFFF));
        break;
    case IEEE80211_PARAM_OCE_HLP:
         retv = wlan_set_param(vap, IEEE80211_OCE_HLP, !!value);
         break;
    case IEEE80211_PARAM_NBR_SCAN_PERIOD:
        vap->nbr_scan_period = value;
        break;
    case IEEE80211_PARAM_RNR:
        vap->rnr_enable = !!value;
        if (vap->iv_bcn_offload_enable && !vap->rnr_enable)
            ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
        break;
    case IEEE80211_PARAM_RNR_FD:
        vap->rnr_enable_fd = !!value;
        break;
    case IEEE80211_PARAM_RNR_TBTT:
        vap->rnr_enable_tbtt = !!value;
        break;
    case IEEE80211_PARAM_AP_CHAN_RPT:
        vap->ap_chan_rpt_enable = value;
        if (vap->iv_bcn_offload_enable && !vap->ap_chan_rpt_enable)
            ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
        break;
    case IEEE80211_PARAM_RRM_CAP:
        retv = wlan_set_param(vap, IEEE80211_RRM_CAP, !!value);
        if (retv == EOK)
            retv = ENETRESET;
        break;
    case IEEE80211_PARAM_RRM_DEBUG:
        retv = wlan_set_param(vap, IEEE80211_RRM_DEBUG, value);
        break;
    case IEEE80211_PARAM_RRM_STATS:
        retv = wlan_set_param(vap, IEEE80211_RRM_STATS, !!value);
	break;
    case IEEE80211_PARAM_RRM_SLWINDOW:
        retv = wlan_set_param(vap, IEEE80211_RRM_SLWINDOW, !!value);
        break;
#if UMAC_SUPPORT_WNM
    case IEEE80211_PARAM_WNM_CAP:
        if (value > 1 || value < 0) {
            qdf_print(" ERR :- Invalid value %d Value to be either 0 or 1 ", value);
            return -EINVAL;
        } else {
            retv = wlan_set_param(vap, IEEE80211_WNM_CAP, value);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
         if (ic->nss_vops)
            ic->nss_vops->ic_osif_nss_vdev_set_cfg(osifp, OSIF_NSS_WIFI_VDEV_CFG_WNM_CAP);
#endif
            if (retv == EOK)
                retv = ENETRESET;
        }
        break;
     case IEEE80211_PARAM_WNM_BSS_CAP: /* WNM Max BSS idle */
         if (value > 1 || value < 0) {
             qdf_print(" ERR :- Invalid value %d Value to be either 0 or 1 ", value);
             return -EINVAL;
         } else {
             retv = wlan_set_param(vap, IEEE80211_WNM_BSS_CAP, value);
             if (retv == EOK)
                 retv = ENETRESET;
         }
         break;
     case IEEE80211_PARAM_WNM_TFS_CAP:
         if (value > 1 || value < 0) {
             qdf_print(" ERR :- Invalid value %d Value to be either 0 or 1 ", value);
             return -EINVAL;
         } else {
             retv = wlan_set_param(vap, IEEE80211_WNM_TFS_CAP, value);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
         if (ic->nss_vops)
             ic->nss_vops->ic_osif_nss_vdev_set_cfg(osifp, OSIF_NSS_WIFI_VDEV_CFG_WNM_TFS);
#endif
             if (retv == EOK)
                 retv = ENETRESET;
         }
         break;
     case IEEE80211_PARAM_WNM_TIM_CAP:
         if (value > 1 || value < 0) {
             qdf_print(" ERR :- Invalid value %d Value to be either 0 or 1 ", value);
             return -EINVAL;
         } else {
             retv = wlan_set_param(vap, IEEE80211_WNM_TIM_CAP, value);
             if (retv == EOK)
                 retv = ENETRESET;
         }
         break;
     case IEEE80211_PARAM_WNM_SLEEP_CAP:
         if (value > 1 || value < 0) {
             qdf_print(" ERR :- Invalid value %d Value to be either 0 or 1 ", value);
             return -EINVAL;
         } else {
             retv = wlan_set_param(vap, IEEE80211_WNM_SLEEP_CAP, value);
             if (retv == EOK)
                 retv = ENETRESET;
         }
         break;
    case IEEE80211_PARAM_WNM_FMS_CAP:
        if (value > 1 || value < 0) {
            return -EINVAL;
        } else {
            retv = wlan_set_param(vap, IEEE80211_WNM_FMS_CAP, value);
            if (retv == EOK)
                retv = ENETRESET;
        }
        break;
#endif
    case IEEE80211_PARAM_PWRTARGET:
        retv = wlan_set_device_param(ic, IEEE80211_DEVICE_PWRTARGET, value);
        break;
    case IEEE80211_PARAM_WMM:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_WMM, value);
        if (retv != EOK)
            break;

        /* AMPDU should reflect changes to WMM */
        if (value && ic->ic_set_config_enable(vap))
            value = CUSTOM_AGGR_MAX_AMPDU_SIZE; //Lithium
        else if (value)
            value = IEEE80211_AMPDU_SUBFRAME_MAX;

        /* Notice fallthrough */
    case IEEE80211_PARAM_AMPDU:
        if (osifp->osif_is_mode_offload) {
            uint32_t ampdu;

            ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_AMPDU, &ampdu);
            /* configure the max ampdu subframes */
                prev_state = ampdu ? 1:0;
            retv = wlan_set_param(vap, IEEE80211_AMPDU_SET, value);
            if (retv == -EINVAL)
                return -EINVAL;
            else {
                ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_AMPDU, &ampdu);
                new_state = ampdu ? 1:0;
            }
        } else {
            if (value > IEEE80211_AMPDU_SUBFRAME_MAX || value < 0) {
                qdf_print("AMPDU value range is 0 - %d", IEEE80211_AMPDU_SUBFRAME_MAX);
                return -EINVAL;
            } else {
                uint32_t ampdu;

                ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_AMPDU, &ampdu);
                prev_state = ampdu ? 1:0;
                retv = wlan_set_param(vap, IEEE80211_FEATURE_AMPDU, value);

                ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_AMPDU, &ampdu);
                new_state = ampdu ? 1:0;
            }
        }

        if (retv == EOK) {
            retv = ENETRESET;
        }

#if ATH_SUPPORT_IBSS_HT
        /*
         * config ic adhoc AMPDU capability
         */
        if (vap->iv_opmode == IEEE80211_M_IBSS) {

            wlan_dev_t ic = wlan_vap_get_devhandle(vap);

            if (value &&
               (ieee80211_ic_ht20Adhoc_is_set(ic) || ieee80211_ic_ht40Adhoc_is_set(ic))) {
                wlan_set_device_param(ic, IEEE80211_DEVICE_HTADHOCAGGR, 1);
                qdf_print("%s IEEE80211_PARAM_AMPDU = %d and HTADHOC enable", __func__, value);
            } else {
                wlan_set_device_param(ic, IEEE80211_DEVICE_HTADHOCAGGR, 0);
                qdf_print("%s IEEE80211_PARAM_AMPDU = %d and HTADHOC disable", __func__, value);
            }
            if ((prev_state) && (!new_state)) {
                retv = ENETRESET;
            } else {
                // don't reset
                retv = EOK;
            }
        }
#endif /* end of #if ATH_SUPPORT_IBSS_HT */

        break;
#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME
    case IEEE80211_PARAM_REJOINT_ATTEMP_TIME:
        retv = wlan_set_param(vap,IEEE80211_REJOINT_ATTEMP_TIME,value);
        break;
#endif

        case IEEE80211_PARAM_AMSDU:
#if defined(TEMP_AGGR_CFG)
        if (!value) {
            if (!osifp->osif_is_mode_offload) {
                retv = wlan_set_param(vap, IEEE80211_AMSDU_SET, value);
            }
            ieee80211vap_clear_flag_ext(vap, IEEE80211_FEXT_AMSDU);
        } else {
            if (!osifp->osif_is_mode_offload) {
                if (value == 1) {
                    retv = wlan_set_param(vap, IEEE80211_AMSDU_SET, value);
                } else {
                    qdf_print("AMSDU can only be toggled 0 or 1");
                    return -EINVAL;
                }
            }
            ieee80211vap_set_flag_ext(vap, IEEE80211_FEXT_AMSDU);
        }

        if (osifp->osif_is_mode_offload) {
            /* configure the max amsdu subframes */
            retv = wlan_set_param(vap, IEEE80211_AMSDU_SET, value);
        }
#endif
        break;

        case IEEE80211_PARAM_RATE_DROPDOWN:
            if (osifp->osif_is_mode_offload) {
                if ((value >= 0) && (value <= RATE_DROPDOWN_LIMIT)) {
                        vap->iv_ratedrop = value;
                        retv = ic->ic_vap_set_param(vap, IEEE80211_RATE_DROPDOWN_SET, value);
                } else {
                        qdf_print("Rate Control Logic is [0-7]");
                        retv = -EINVAL;
                }
            } else {
                qdf_print("This Feature is Supported for Offload Mode Only");
                return -EINVAL;
            }
        break;

    case IEEE80211_PARAM_11N_TX_AMSDU:
        /* Enable/Disable Tx AMSDU for HT clients. Sanitise to 0 or 1 only */
        vap->iv_disable_ht_tx_amsdu = !!value;
        break;

    case IEEE80211_PARAM_CTSPROT_DTIM_BCN:
        retv = wlan_set_vap_cts2self_prot_dtim_bcn(vap, !!value);
        if (!retv)
            retv = EOK;
        else
            retv = -EINVAL;
       break;

    case IEEE80211_PARAM_VSP_ENABLE:
        /* Enable/Disable VSP for VOW */
        vap->iv_enable_vsp = !!value;
        break;

    case IEEE80211_PARAM_SHORTPREAMBLE:
        retv = wlan_set_param(vap, IEEE80211_SHORT_PREAMBLE, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
       break;

    case IEEE80211_PARAM_CHANBW:
        switch (value)
        {
        case 0:
            ic->ic_chanbwflag = 0;
            break;
        case 1:
            ic->ic_chanbwflag = IEEE80211_CHAN_HALF;
            break;
        case 2:
            ic->ic_chanbwflag = IEEE80211_CHAN_QUARTER;
            break;
        default:
            retv = -EINVAL;
            break;
        }

       /*
        * bandwidth change need reselection of channel based on the chanbwflag
        * This is required if the command is issued after the freq has been set
        * neither the chanbw param does not take effect
        */
       if ( retv == 0 ) {
           deschan = wlan_get_param(vap, IEEE80211_DESIRED_CHANNEL);
           retv  = wlan_set_channel(vap, deschan, vap->iv_des_cfreq2);

           if (retv == 0) {
               /*Reinitialize the vap*/
               retv = ENETRESET ;
           }
       }
#if UMAC_SUPPORT_CFG80211
       wlan_cfg80211_update_channel_list(ic);
#endif
        break;

    case IEEE80211_PARAM_INACT:
        wlan_set_param(vap, IEEE80211_RUN_INACT_TIMEOUT, value);
        break;
    case IEEE80211_PARAM_INACT_AUTH:
        wlan_set_param(vap, IEEE80211_AUTH_INACT_TIMEOUT, value);
        break;
    case IEEE80211_PARAM_INACT_INIT:
        wlan_set_param(vap, IEEE80211_INIT_INACT_TIMEOUT, value);
        break;
    case IEEE80211_PARAM_SESSION_TIMEOUT:
        wlan_set_param(vap, IEEE80211_SESSION_TIMEOUT, value);
        break;
    case IEEE80211_PARAM_WDS_AUTODETECT:
        wlan_set_param(vap, IEEE80211_WDS_AUTODETECT, value);
        break;
    case IEEE80211_PARAM_WEP_TKIP_HT:
		wlan_set_param(vap, IEEE80211_WEP_TKIP_HT, value);
        retv = ENETRESET;
        break;
    case IEEE80211_PARAM_IGNORE_11DBEACON:
        wlan_set_param(vap, IEEE80211_IGNORE_11DBEACON, value);
        break;
    case IEEE80211_PARAM_MFP_TEST:
        wlan_set_param(vap, IEEE80211_FEATURE_MFP_TEST, value);
        break;

#ifdef QCA_PARTNER_PLATFORM
    case IEEE80211_PARAM_PLTFRM_PRIVATE:
        retv = wlan_pltfrm_set_param(vap, value);
 	    if ( retv == EOK) {
 	        retv = ENETRESET;
 	    }
 	    break;
#endif

    case IEEE80211_PARAM_NO_STOP_DISASSOC:
        if (value)
            osifp->no_stop_disassoc = 1;
        else
            osifp->no_stop_disassoc = 0;
        break;
#if UMAC_SUPPORT_VI_DBG

        case IEEE80211_PARAM_DBG_CFG:
	    osifp->vi_dbg = value;
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_DBG_CFG, value);
            break;

        case IEEE80211_PARAM_RESTART:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RESTART, value);
            break;
        case IEEE80211_PARAM_RXDROP_STATUS:
            ieee80211_vi_dbg_set_param(vap, IEEE80211_VI_RXDROP_STATUS, value);
            break;
#endif
    case IEEE80211_IOC_WPS_MODE:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                        "set IEEE80211_IOC_WPS_MODE to 0x%x\n", value);
        retv = wlan_set_param(vap, IEEE80211_WPS_MODE, value);
        break;

    case IEEE80211_IOC_SCAN_FLUSH:
#if ATH_SUPPORT_WRAP
        /*Avoid flushing scan results in proxysta case as proxysta needs to
          use the scan results of main-proxysta */
        if (vap->iv_no_event_handler) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set %s\n",
                "Bypass IEEE80211_IOC_SCAN_FLUSH for non main-proxysta");
            break;
        }
#endif
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "set %s\n",
                        "IEEE80211_IOC_SCAN_FLUSH");
        status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_OSIF_SCAN_ID);
        if (QDF_IS_STATUS_ERROR(status)) {
            scan_info("unable to get reference");
            retv = -EBUSY;
            break;
        }
        ucfg_scan_flush_results(pdev, NULL);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_OSIF_SCAN_ID);
        retv = 0; /* success */
        break;

#ifdef ATH_SUPPORT_TxBF
    case IEEE80211_PARAM_TXBF_AUTO_CVUPDATE:
        wlan_set_param(vap, IEEE80211_TXBF_AUTO_CVUPDATE, value);
        ic->ic_set_config(vap);
        break;
    case IEEE80211_PARAM_TXBF_CVUPDATE_PER:
        wlan_set_param(vap, IEEE80211_TXBF_CVUPDATE_PER, value);
        ic->ic_set_config(vap);
        break;
#endif
    case IEEE80211_PARAM_SCAN_BAND:
        if ((value == OSIF_SCAN_BAND_2G_ONLY  && IEEE80211_SUPPORT_PHY_MODE(ic,IEEE80211_MODE_11G)) ||
            (value == OSIF_SCAN_BAND_5G_ONLY  && IEEE80211_SUPPORT_PHY_MODE(ic,IEEE80211_MODE_11A)) ||
            (value == OSIF_SCAN_BAND_ALL))
        {
            osifp->os_scan_band = value;
        }
        retv = 0;
        break;

    case IEEE80211_PARAM_SCAN_CHAN_EVENT:
        if (osifp->osif_is_mode_offload &&
            wlan_vap_get_opmode(vap) == IEEE80211_M_HOSTAP) {
            osifp->is_scan_chevent = !!value;
            retv = 0;
        } else {
            qdf_print("IEEE80211_PARAM_SCAN_CHAN_EVENT is valid only for 11ac "
                   "offload, and in IEEE80211_M_HOSTAP(Access Point) mode");
            retv = -EOPNOTSUPP;
        }
        break;

#if UMAC_SUPPORT_PROXY_ARP
    case IEEE80211_PARAM_PROXYARP_CAP:
        wlan_set_param(vap, IEEE80211_PROXYARP_CAP, value);
	    break;
#if UMAC_SUPPORT_DGAF_DISABLE
    case IEEE80211_PARAM_DGAF_DISABLE:
        wlan_set_param(vap, IEEE80211_DGAF_DISABLE, value);
        break;
#endif
#endif
#if UMAC_SUPPORT_HS20_L2TIF
    case IEEE80211_PARAM_L2TIF_CAP:
        value = value ? 0 : 1;
        wlan_set_param(vap, IEEE80211_FEATURE_APBRIDGE, value);
       break;
#endif
    case IEEE80211_PARAM_EXT_IFACEUP_ACS:
        wlan_set_param(vap, IEEE80211_EXT_IFACEUP_ACS, value);
        break;

    case IEEE80211_PARAM_EXT_ACS_IN_PROGRESS:
        wlan_set_param(vap, IEEE80211_EXT_ACS_IN_PROGRESS, value);
        break;

    case IEEE80211_PARAM_SEND_ADDITIONAL_IES:
        wlan_set_param(vap, IEEE80211_SEND_ADDITIONAL_IES, value);
        break;

    case IEEE80211_PARAM_APONLY:
#if UMAC_SUPPORT_APONLY
        vap->iv_aponly = value ? true : false;
        ic->ic_aponly = vap->iv_aponly;
#else
        qdf_print("APONLY not enabled");
#endif
        break;
    case IEEE80211_PARAM_ONETXCHAIN:
        vap->iv_force_onetxchain = value ? true : false;
        break;

    case IEEE80211_PARAM_SET_CABQ_MAXDUR:
        if (value > 0 && value < 100)
            wlan_set_param(vap, IEEE80211_SET_CABQ_MAXDUR, value);
        else
            qdf_print("Percentage should be between 0 and 100");
        break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    case IEEE80211_PARAM_NOPBN:
        wlan_set_param(vap, IEEE80211_NOPBN, value);
        break;
#endif
#if ATH_SUPPORT_DSCP_OVERRIDE
    case IEEE80211_PARAM_DSCP_MAP_ID:
        qdf_print("Set DSCP override %d",value);
        wlan_set_param(vap, IEEE80211_DSCP_MAP_ID, value);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_vops)
        ic->nss_vops->ic_osif_nss_vdev_set_cfg(osifp, OSIF_NSS_WIFI_VDEV_CFG_DSCP_OVERRIDE);
#endif
        break;
    case IEEE80211_PARAM_DSCP_TID_MAP:
        qdf_print("Set vap dscp tid map");
        wlan_set_vap_dscp_tid_map(osifp->os_if, val[1], val[2]);
        break;
#endif
    case IEEE80211_PARAM_TXRX_VAP_STATS:
        if (!ic->ic_is_mode_offload(ic)) {
            qdf_print("TXRX_DBG Only valid for 11ac  ");
        } else {
	    retv = ic->ic_vap_set_param(vap, IEEE80211_TXRX_VAP_STATS, value);
        }
        break;
    case IEEE80211_PARAM_TXRX_DBG:
        if(!ic->ic_is_mode_offload(ic)) {
            qdf_print("TXRX_DBG Only valid for 11ac  ");
        } else {
	    retv = ic->ic_vap_set_param(vap, IEEE80211_TXRX_DBG_SET, value);
        }
        break;
    case IEEE80211_PARAM_VAP_TXRX_FW_STATS:
        if (!ic->ic_is_mode_offload(ic)) {
            qdf_print("FW_STATS Only valid for offload radios ");
            return -EINVAL ;
        } else {
            retv = ic->ic_vap_set_param(vap, IEEE80211_VAP_TXRX_FW_STATS, value);
        }
        break;
    case IEEE80211_PARAM_TXRX_FW_STATS:
        if(!ic->ic_is_mode_offload(ic)) {
            qdf_print("FW_STATS Only valid for 11ac  ");
        } else {
            retv = ic->ic_vap_set_param(vap, IEEE80211_TXRX_FW_STATS, value);
        }
        break;
    case IEEE80211_PARAM_TXRX_FW_MSTATS:
        if(!ic->ic_is_mode_offload(ic)) {
           qdf_print("FW_MSTATS Only valid for 11ac  ");
        } else {
           retv = ic->ic_vap_set_param(vap, IEEE80211_TXRX_FW_MSTATS, value);
        }
        break;
    case IEEE80211_PARAM_VAP_TXRX_FW_STATS_RESET:
       if (!ic->ic_is_mode_offload(ic)) {
           qdf_print("FW_STATS_RESET Only valid for offload radios  ");
           return -EINVAL;
       } else {
           retv = ic->ic_vap_set_param(vap, IEEE80211_VAP_TXRX_FW_STATS_RESET, value);
       }
       break;
    case IEEE80211_PARAM_TXRX_FW_STATS_RESET:
       if (!ic->ic_is_mode_offload(ic)) {
            qdf_print("FW_STATS_RESET Only valid for 11ac  ");
       }
       else {
           retv = ic->ic_vap_set_param(vap, IEEE80211_TXRX_FW_STATS_RESET, value);
       }
       break;
    case IEEE80211_PARAM_TXRX_DP_STATS:
       retv = ic->ic_vap_set_param(vap, IEEE80211_TXRX_DP_STATS, value);
       break;
    case IEEE80211_PARAM_TX_PPDU_LOG_CFG:
        if (!ic->ic_is_mode_offload(ic)) {
            qdf_print("TX_PPDU_LOG_CFG  Only valid for 11ac  ");
            break;
        }
        retv = ic->ic_vap_set_param(vap, IEEE80211_TX_PPDU_LOG_CFG_SET, value);
        break;
    case IEEE80211_PARAM_MAX_SCANENTRY:
        retv = wlan_set_param(vap, IEEE80211_MAX_SCANENTRY, value);
        break;
    case IEEE80211_PARAM_SCANENTRY_TIMEOUT:
        retv = wlan_set_param(vap, IEEE80211_SCANENTRY_TIMEOUT, value);
        break;
#if ATH_PERF_PWR_OFFLOAD && QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    case IEEE80211_PARAM_CLR_RAWMODE_PKT_SIM_STATS:
        retv = wlan_set_param(vap, IEEE80211_CLR_RAWMODE_PKT_SIM_STATS, value);
        break;
#endif /* ATH_PERF_PWR_OFFLOAD && QCA_SUPPORT_RAWMODE_PKT_SIMULATION */
    default:
        retv = -EOPNOTSUPP;
        if (retv) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s parameter 0x%x is "
                            "not supported retv=%d\n", __func__, param, retv);
        }
        break;
#if ATH_SUPPORT_IBSS_DFS
     case IEEE80211_PARAM_IBSS_DFS_PARAM:
        {
#define IBSSDFS_CSA_TIME_MASK 0x00ff0000
#define IBSSDFS_ACTION_MASK   0x0000ff00
#define IBSSDFS_RECOVER_MASK  0x000000ff
            u_int8_t csa_in_tbtt;
            u_int8_t actions_threshold;
            u_int8_t rec_threshold_in_tbtt;

            csa_in_tbtt = (value & IBSSDFS_CSA_TIME_MASK) >> 16;
            actions_threshold = (value & IBSSDFS_ACTION_MASK) >> 8;
            rec_threshold_in_tbtt = (value & IBSSDFS_RECOVER_MASK);

            if (rec_threshold_in_tbtt > csa_in_tbtt &&
                actions_threshold > 0) {
                vap->iv_ibss_dfs_csa_threshold = csa_in_tbtt;
                vap->iv_ibss_dfs_csa_measrep_limit = actions_threshold;
                vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt = rec_threshold_in_tbtt;
                ieee80211_ibss_beacon_update_start(ic);
            } else {
                qdf_print("please enter a valid value .ex 0x010102");
                qdf_print("Ex.0xaabbcc aa[channel switch time] bb[actions count] cc[recovery time]");
                qdf_print("recovery time must be bigger than channel switch time, actions count must > 0");
            }

#undef IBSSDFS_CSA_TIME_MASK
#undef IBSSDFS_ACTION_MASK
#undef IBSSDFS_RECOVER_MASK
        }
        break;
#endif
#if ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
    case IEEE80211_PARAM_IBSS_SET_RSSI_CLASS:
      {
	int i;
	u_int8_t rssi;
	u_int8_t *pvalue = (u_int8_t*)(extra + 4);

	/* 0th idx is 0 dbm(highest) always */
	vap->iv_ibss_rssi_class[0] = (u_int8_t)-1;

	for( i = 1; i < IBSS_RSSI_CLASS_MAX; i++ ) {
	  rssi = pvalue[i - 1];
	  /* Assumes the values in dbm are already sorted.
	   * Convert to rssi and store them */
	  vap->iv_ibss_rssi_class[i] = (rssi > 95 ? 0 : (95 - rssi));
	}
      }
      break;
    case IEEE80211_PARAM_IBSS_START_RSSI_MONITOR:
      vap->iv_ibss_rssi_monitor = value;
      /* set the hysteresis to atleast 1 */
      if (value && !vap->iv_ibss_rssi_hysteresis)
	vap->iv_ibss_rssi_hysteresis++;
      break;
    case IEEE80211_PARAM_IBSS_RSSI_HYSTERESIS:
      vap->iv_ibss_rssi_hysteresis = value;
        break;
#endif

#if ATH_SUPPORT_WIFIPOS
   case IEEE80211_PARAM_WIFIPOS_TXCORRECTION:
	ieee80211_wifipos_set_txcorrection(vap, value);
   	break;

   case IEEE80211_PARAM_WIFIPOS_RXCORRECTION:
	ieee80211_wifipos_set_rxcorrection(vap, value);
   	break;
#endif

   case IEEE80211_PARAM_DFS_CACTIMEOUT:
#if ATH_SUPPORT_DFS
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
            QDF_STATUS_SUCCESS) {
        return -1;
    }
    retv = mlme_dfs_override_cac_timeout(pdev, value);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
    if (retv != 0)
        retv = -EOPNOTSUPP;
    break;
#else
        retv = -EOPNOTSUPP;
    break;
#endif /* ATH_SUPPORT_DFS */

   case IEEE80211_PARAM_ENABLE_RTSCTS:
       retv = wlan_set_param(vap, IEEE80211_ENABLE_RTSCTS, value);
   break;
    case IEEE80211_PARAM_MAX_AMPDU:
        if ((value >= IEEE80211_MAX_AMPDU_MIN) &&
            (value <= IEEE80211_MAX_AMPDU_MAX)) {
            retv = wlan_set_param(vap, IEEE80211_MAX_AMPDU, value);
 	        if ( retv == EOK ) {
                retv = ENETRESET;
            }
        } else {
            retv = -EINVAL;
        }
        break;
    case IEEE80211_PARAM_VHT_MAX_AMPDU:
        if ((value >= IEEE80211_VHT_MAX_AMPDU_MIN) &&
            (value <= IEEE80211_VHT_MAX_AMPDU_MAX)) {
            retv = wlan_set_param(vap, IEEE80211_VHT_MAX_AMPDU, value);
            if ( retv == EOK ) {
                retv = ENETRESET;
            }
        } else {
            retv = -EINVAL;
        }
        break;
    case IEEE80211_PARAM_IMPLICITBF:
        retv = wlan_set_param(vap, IEEE80211_SUPPORT_IMPLICITBF, value);
        if ( retv == EOK ) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_VHT_SUBFEE:
        if (value == 0 ) {
            /* if SU is disabled, disable MU as well */
            wlan_set_param(vap, IEEE80211_VHT_MUBFEE, value);
        }
        retv = wlan_set_param(vap, IEEE80211_VHT_SUBFEE, value);
        if (retv == EOK && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_VHT_MUBFEE:
        if (value == 1) {
            /* if MU is enabled, enable SU as well */
            wlan_set_param(vap, IEEE80211_VHT_SUBFEE, value);
        }
        retv = wlan_set_param(vap, IEEE80211_VHT_MUBFEE, value);
        if (retv == EOK && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_VHT_SUBFER:
        if (value == 0 ) {
            /* if SU is disabled, disable MU as well */
            wlan_set_param(vap, IEEE80211_VHT_MUBFER, value);
        }
        retv = wlan_set_param(vap, IEEE80211_VHT_SUBFER, value);
        if (retv == EOK && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_VHT_MUBFER:
        if (value == 1 ) {
            /* if MU is enabled, enable SU as well */
            wlan_set_param(vap, IEEE80211_VHT_SUBFER, value);
        }
        retv = wlan_set_param(vap, IEEE80211_VHT_MUBFER, value);
        if (retv == 0 && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_VHT_STS_CAP:
        retv = wlan_set_param(vap, IEEE80211_VHT_BF_STS_CAP, value);
        if (retv == 0)
            retv = ENETRESET;
        break;

    case IEEE80211_PARAM_VHT_SOUNDING_DIM:
        retv = wlan_set_param(vap, IEEE80211_VHT_BF_SOUNDING_DIM, value);
        if (retv == 0)
            retv = ENETRESET;
        break;
    case IEEE80211_PARAM_VHT_MCS_10_11_SUPP:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_VHT_MCS_10_11_SUPP, value);
        if(retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_VHT_MCS_10_11_NQ2Q_PEER_SUPP:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_VHT_MCS_10_11_NQ2Q_PEER_SUPP, value);
        if(retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_RC_NUM_RETRIES:
        retv = wlan_set_param(vap, IEEE80211_RC_NUM_RETRIES, value);
        break;
    case IEEE80211_PARAM_256QAM_2G:
        retv = wlan_set_param(vap, IEEE80211_256QAM, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_11NG_VHT_INTEROP:
        if (osifp->osif_is_mode_offload) {
            retv = wlan_set_param(vap, IEEE80211_11NG_VHT_INTEROP , value);
            if (retv == EOK) {
                retv = ENETRESET;
            }
        } else {
            qdf_print("Not supported in this vap ");
        }
        break;
#if UMAC_VOW_DEBUG
    case IEEE80211_PARAM_VOW_DBG_ENABLE:
        {
            osifp->vow_dbg_en = value;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (ic->nss_vops)
            ic->nss_vops->ic_osif_nss_vdev_set_cfg(osifp, OSIF_NSS_WIFI_VDEV_VOW_DBG_MODE);
#endif
        }
        break;
#endif

#if WLAN_SUPPORT_SPLITMAC
    case IEEE80211_PARAM_SPLITMAC:
        splitmac_set_enabled_flag(vdev, !!value);

        if (splitmac_is_enabled(vdev)) {
            osifp->app_filter =  IEEE80211_FILTER_TYPE_ALL;
            wlan_set_param(vap, IEEE80211_TRIGGER_MLME_RESP, 1);
        } else {
            osifp->app_filter =  0;
            wlan_set_param(vap, IEEE80211_TRIGGER_MLME_RESP, 0);
        }
        break;
#endif
#if ATH_PERF_PWR_OFFLOAD
    case IEEE80211_PARAM_VAP_TX_ENCAP_TYPE:
        retv = wlan_set_param(vap, IEEE80211_VAP_TX_ENCAP_TYPE, value);
        if(retv == EOK) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_VAP_RX_DECAP_TYPE:
        retv = wlan_set_param(vap, IEEE80211_VAP_RX_DECAP_TYPE, value);
        if(retv == EOK) {
            retv = ENETRESET;
        }
        break;
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    case IEEE80211_PARAM_RAWMODE_SIM_TXAGGR:
        retv = wlan_set_param(vap, IEEE80211_RAWMODE_SIM_TXAGGR, value);
        break;
    case IEEE80211_PARAM_RAWMODE_SIM_DEBUG:
        retv = wlan_set_param(vap, IEEE80211_RAWMODE_SIM_DEBUG, value);
        break;
#endif /* QCA_SUPPORT_RAWMODE_PKT_SIMULATION */
#endif /* ATH_PERF_PWR_OFFLOAD */
    case IEEE80211_PARAM_STA_FIXED_RATE:
        /* set a fixed data rate for an associated STA on a AP vap.
         * assumes that vap is already enabled for fixed rate, and
         * this setting overrides the vap's setting.
         *
         * encoding (legacey)  : ((aid << 8)  |
         *                          (preamble << 6) | ((nss-1) << 4) | mcs)
         * encoding (he_target): ((aid << 16) |
         *                          (preamble << 8) | ((nss-1) << 5) | mcs)
         * preamble (OFDM) = 0x0
         * preamble (CCK)  = 0x1
         * preamble (HT)   = 0x2
         * preamble (VHT)  = 0x3
         * preamble (HE)   = 0x4
         */
        if (osifp->os_opmode != IEEE80211_M_HOSTAP) {
            return -EINVAL;
        }

        if (IEEE80211_VAP_IN_FIXED_RATE_MODE(vap)) {
            struct find_wlan_node_req req;
            if (!ic->ic_he_target) {
                req.assoc_id = ((value >> RATECODE_LEGACY_RC_SIZE)
                                        & IEEE80211_ASSOC_ID_MASK);
            } else {
                req.assoc_id = ((value >> RATECODE_V1_RC_SIZE)
                                        & IEEE80211_ASSOC_ID_MASK);
            }
            req.node = NULL;
            wlan_iterate_station_list(vap, find_wlan_node_by_associd, &req);
            if (req.node) {
                uint32_t fixed_rate;
                if (!ic->ic_he_target) {
                    fixed_rate = value & RATECODE_LEGACY_RC_MASK;
                } else {
                    fixed_rate = value & RATECODE_V1_RC_MASK;
                    /* V1 rate code format is ((((1) << 28) |(pream) << 8)
                     * | ((nss) << 5) | (rate)). With this command, user
                     * will send us  16 bit _rate = (((_pream) << 8) | ((nss)
                     * << 5) | rate). We need to assemble user sent _rate as
                     * V1 rate = (((1) << 28) | _rate).
                     */
                    fixed_rate = V1_RATECODE_FROM_RATE(fixed_rate);
                }
                retv = wlan_node_set_fixed_rate(req.node, fixed_rate);
            } else {
                return -EINVAL;
            }
        }
        break;

#if QCA_AIRTIME_FAIRNESS
    case IEEE80211_PARAM_ATF_TXBUF_SHARE:
        retv = ucfg_atf_set_txbuf_share(vap->vdev_obj, value);
        break;
    case IEEE80211_PARAM_ATF_TXBUF_MAX:
        ucfg_atf_set_max_txbufs(vap->vdev_obj, value);
        break;
    case IEEE80211_PARAM_ATF_TXBUF_MIN:
        ucfg_atf_set_min_txbufs(vap->vdev_obj, value);
        break;
    case  IEEE80211_PARAM_ATF_OPT:
        if (value > 1) {
            qdf_print("update commit value(1) wrong ");
            return -EINVAL;
        }
        if (value)
            retv = ucfg_atf_set(vap->vdev_obj);
        else
            retv = ucfg_atf_clear(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_ATF_OVERRIDE_AIRTIME_TPUT:
        ucfg_atf_set_airtime_tput(vap->vdev_obj, value);
        break;
    case  IEEE80211_PARAM_ATF_PER_UNIT:
        ucfg_atf_set_per_unit(ic->ic_pdev_obj);
        break;
    case  IEEE80211_PARAM_ATF_MAX_CLIENT:
        retv = ucfg_atf_set_maxclient(ic->ic_pdev_obj, value);
    break;
    case  IEEE80211_PARAM_ATF_SSID_GROUP:
        ucfg_atf_set_ssidgroup(ic->ic_pdev_obj, vap->vdev_obj, value);
    break;
    case IEEE80211_PARAM_ATF_SSID_SCHED_POLICY:
        retv = ucfg_atf_set_ssid_sched_policy(vap->vdev_obj, value);
    break;
#endif
#if ATH_SSID_STEERING
    case IEEE80211_PARAM_VAP_SSID_CONFIG:
        retv = wlan_set_param(vap, IEEE80211_VAP_SSID_CONFIG, value);
        break;
#endif
    case IEEE80211_PARAM_RX_FILTER_MONITOR:
        if(IEEE80211_M_MONITOR != vap->iv_opmode && !vap->iv_smart_monitor_vap && !vap->iv_special_vap_mode) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Not monitor VAP or Smart monitor VAP!\n");
            return -EINVAL;
        }
        retv = wlan_set_param(vap, IEEE80211_RX_FILTER_MONITOR, value);
        break;
    case  IEEE80211_PARAM_RX_FILTER_NEIGHBOUR_PEERS_MONITOR:
        /* deliver configured bss peer packets, associated to smart
         * monitor vap and filter out other valid/invalid peers
         */
        if(!vap->iv_smart_monitor_vap) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Not smart monitor VAP!\n");
            return -EINVAL;
        }
        retv = wlan_set_param(vap, IEEE80211_RX_FILTER_NEIGHBOUR_PEERS_MONITOR, value);
        break;
    case IEEE80211_PARAM_AMPDU_DENSITY_OVERRIDE:
        if(value < 0) {
            ic->ic_mpdudensityoverride = 0;
        } else if(value <= IEEE80211_HTCAP_MPDUDENSITY_MAX) {
            /* mpdudensityoverride
             * Bits
             * 7 --  4   3  2  1    0
             * +------------------+----+
             * |       |  MPDU    |    |
             * | Rsvd  | DENSITY  |E/D |
             * +---------------+-------+
             */
            ic->ic_mpdudensityoverride = 1;
            ic->ic_mpdudensityoverride |= ((u_int8_t)value & 0x07) << 1;
        } else {
            qdf_print("Usage:\n"
                    "-1 - Disable mpdu density override \n"
                    "%d - No restriction \n"
                    "%d - 1/4 usec \n"
                    "%d - 1/2 usec \n"
                    "%d - 1 usec \n"
                    "%d - 2 usec \n"
                    "%d - 4 usec \n"
                    "%d - 8 usec \n"
                    "%d - 16 usec ",
                    IEEE80211_HTCAP_MPDUDENSITY_NA,
                    IEEE80211_HTCAP_MPDUDENSITY_0_25,
                    IEEE80211_HTCAP_MPDUDENSITY_0_5,
                    IEEE80211_HTCAP_MPDUDENSITY_1,
                    IEEE80211_HTCAP_MPDUDENSITY_2,
                    IEEE80211_HTCAP_MPDUDENSITY_4,
                    IEEE80211_HTCAP_MPDUDENSITY_8,
                    IEEE80211_HTCAP_MPDUDENSITY_16);
            retv = EINVAL;
        }
        /* Reset VAP */
        if(retv != EINVAL) {
            wlan_chan_t chan = wlan_get_bss_channel(vap);
            if (chan != IEEE80211_CHAN_ANYC) {
                retv = ENETRESET;
            }
        }
        break;

    case IEEE80211_PARAM_SMART_MESH_CONFIG:
        retv = wlan_set_param(vap, IEEE80211_SMART_MESH_CONFIG, value);
        break;
#if MESH_MODE_SUPPORT
    case IEEE80211_PARAM_MESH_CAPABILITIES:
        retv = wlan_set_param(vap, IEEE80211_MESH_CAPABILITIES, value);
        break;
    case IEEE80211_PARAM_ADD_LOCAL_PEER:
        if (vap->iv_mesh_vap_mode) {
            qdf_print("adding local peer ");
            retv = ieee80211_add_localpeer(vap,extra);
        } else {
            return -EPERM;
        }
        break;
    case IEEE80211_PARAM_SET_MHDR:
        if(vap && vap->iv_mesh_vap_mode) {
            qdf_print("setting mhdr %x",value);
            vap->mhdr = value;
        } else {
            return -EPERM;
        }
        break;
    case IEEE80211_PARAM_ALLOW_DATA:
        if (vap->iv_mesh_vap_mode) {
            qdf_print(" authorise keys ");
            retv = ieee80211_authorise_local_peer(vap,extra);
        } else {
            return -EPERM;
        }
        break;
    case IEEE80211_PARAM_SET_MESHDBG:
        if(vap && vap->iv_mesh_vap_mode) {
            qdf_print("mesh dbg %x",value);
            vap->mdbg = value;
        } else {
            return -EPERM;
        }
        break;
    case IEEE80211_PARAM_CONFIG_MGMT_TX_FOR_MESH:
        if (vap->iv_mesh_vap_mode) {
          retv = wlan_set_param(vap, IEEE80211_CONFIG_MGMT_TX_FOR_MESH, value);
        } else {
            return -EPERM;
        }
        break;

    case IEEE80211_PARAM_MESH_MCAST:
        if (vap->iv_mesh_vap_mode) {
          retv = wlan_set_param(vap, IEEE80211_CONFIG_MESH_MCAST, value);
        } else {
            return -EPERM;
        }
        break;

#if ATH_ACS_DEBUG_SUPPORT
    case IEEE80211_PARAM_ACS_DEBUG_SUPPORT:
        ic->ic_acs_debug_support = value ? 1 : 0;
        break;
#endif

    case IEEE80211_PARAM_CONFIG_RX_MESH_FILTER:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_RX_MESH_FILTER, value);
        break;

#if ATH_DATA_RX_INFO_EN
    case IEEE80211_PARAM_RXINFO_PERPKT:
        vap->rxinfo_perpkt = value;
        break;
#endif
#endif
    case IEEE80211_PARAM_CONFIG_ASSOC_WAR_160W:
        if ((value == 0) || ASSOCWAR160_IS_VALID_CHANGE(value))
            retv = wlan_set_param(vap, IEEE80211_CONFIG_ASSOC_WAR_160W, value);
        else {
            qdf_print("Invalid value %d. Valid bitmap values are 0:Disable, 1:Enable VHT OP, 3:Enable VHT OP and VHT CAP",value);
            return -EINVAL;
        }
 break;
    case IEEE80211_PARAM_WHC_APINFO_SFACTOR:
        ucfg_son_set_scaling_factor(vap->vdev_obj, value);
        break;
    case IEEE80211_PARAM_WHC_SKIP_HYST:
        ucfg_son_set_skip_hyst(vap->vdev_obj, value);
        break;
    case IEEE80211_PARAM_WHC_APINFO_UPLINK_RATE:
        ucfg_son_set_uplink_rate(vap->vdev_obj, value);
	wlan_pdev_beacon_update(ic);
	break;
    case IEEE80211_PARAM_WHC_CAP_RSSI:
	ucfg_son_set_cap_rssi(vap->vdev_obj, value);
	break;
    case IEEE80211_PARAM_SON:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_SON, value);
	son_update_bss_ie(vap->vdev_obj);
	wlan_pdev_beacon_update(ic);
        break;
    case IEEE80211_PARAM_WHC_APINFO_OTHERBAND_BSSID:
        ucfg_son_set_otherband_bssid(vap->vdev_obj, &val[1]);
	son_update_bss_ie(vap->vdev_obj);
	wlan_pdev_beacon_update(ic);
        break;
    case IEEE80211_PARAM_WHC_APINFO_ROOT_DIST:
	ucfg_son_set_root_dist(vap->vdev_obj, value);
	son_update_bss_ie(vap->vdev_obj);
	wlan_pdev_beacon_update(ic);
        break;
    case IEEE80211_PARAM_REPT_MULTI_SPECIAL:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_REPT_MULTI_SPECIAL, value);
        break;
    case IEEE80211_PARAM_RAWMODE_PKT_SIM:
        retv = wlan_set_param(vap, IEEE80211_RAWMODE_PKT_SIM, value);
        break;
    case IEEE80211_PARAM_CONFIG_RAW_DWEP_IND:
        if ((value == 0) || (value == 1))
            retv = wlan_set_param(vap, IEEE80211_CONFIG_RAW_DWEP_IND, value);
        else {
            qdf_print("Invalid value %d. Valid values are 0:Disable, 1:Enable",value);
            return -EINVAL;
        }
        break;
    case IEEE80211_PARAM_CUSTOM_CHAN_LIST:
        retv = wlan_set_param(vap,IEEE80211_CONFIG_PARAM_CUSTOM_CHAN_LIST, value);
        break;
#if UMAC_SUPPORT_ACFG
    case IEEE80211_PARAM_DIAG_WARN_THRESHOLD:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_DIAG_WARN_THRESHOLD, value);
        break;
    case IEEE80211_PARAM_DIAG_ERR_THRESHOLD:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_DIAG_ERR_THRESHOLD, value);
        break;
#endif
     case IEEE80211_PARAM_CONFIG_REV_SIG_160W:
        if(!wlan_get_param(vap, IEEE80211_CONFIG_ASSOC_WAR_160W)){
            if ((value == 0) || (value == 1))
                retv = wlan_set_param(vap, IEEE80211_CONFIG_REV_SIG_160W, value);
            else {
                qdf_print("Invalid value %d. Valid values are 0:Disable, 1:Enable",value);
                return -EINVAL;
            }
        } else {
            qdf_print("revsig160 not supported with assocwar160");
            return -EINVAL;
        }
        break;
     case IEEE80211_PARAM_CONFIG_MU_CAP_WAR:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_MU_CAP_WAR, value);
        break;
     case IEEE80211_PARAM_CONFIG_MU_CAP_TIMER:
        if ((value >= 1) && (value <= 300))
            retv = wlan_set_param(vap, IEEE80211_CONFIG_MU_CAP_TIMER, value);
        else {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                            "Invalid value %d. Valid value is between 1 and 300\n",value);
            return -EINVAL;
        }
        break;
    case IEEE80211_PARAM_DISABLE_SELECTIVE_HTMCS_FOR_VAP:
        if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
            retv = wlan_set_param(vap, IEEE80211_CONFIG_DISABLE_SELECTIVE_HTMCS, value);
            if(retv == 0)
                retv = ENETRESET;
        } else {
            qdf_print("This iwpriv option disable_htmcs is valid only for AP mode vap");
        }
        break;
    case IEEE80211_PARAM_CONFIGURE_SELECTIVE_VHTMCS_FOR_VAP:
        if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
            if(value != 0) {
                retv = wlan_set_param(vap, IEEE80211_CONFIG_CONFIGURE_SELECTIVE_VHTMCS, value);
                if(retv == 0)
                    retv = ENETRESET;
            }
        } else {
            qdf_print("This iwpriv option conf_11acmcs is valid only for AP mode vap");
        }
        break;
    case IEEE80211_PARAM_RDG_ENABLE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_RDG_ENABLE, value);
        break;
    case IEEE80211_PARAM_CLEAR_QOS:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_CLEAR_QOS,value);
        break;
    case IEEE80211_PARAM_TRAFFIC_STATS:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_TRAFFIC_STATS,value);
        break;
    case IEEE80211_PARAM_TRAFFIC_RATE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_TRAFFIC_RATE,value);
        break;
    case IEEE80211_PARAM_TRAFFIC_INTERVAL:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_TRAFFIC_INTERVAL,value);
        break;
    case IEEE80211_PARAM_WATERMARK_THRESHOLD:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_WATERMARK_THRESHOLD,value);
        break;
    case IEEE80211_PARAM_ENABLE_VENDOR_IE:
	if( value == 0 || value == 1) {
	    if (vap->iv_ena_vendor_ie != value) {
                vap->iv_update_vendor_ie = 1;
            }
	    vap->iv_ena_vendor_ie = value;
	} else {
	    qdf_print("%s Enter 1: enable vendor ie, 0: disable vendor ie ",__func__);
	}
	break;
    case IEEE80211_PARAM_CONFIG_ASSOC_DENIAL_NOTIFY:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_ASSOC_DENIAL_NOTIFICATION, value);
        break;
    case IEEE80211_PARAM_MACCMD_SEC:
        retv = wlan_set_acl_policy(vap, value, IEEE80211_ACL_FLAG_ACL_LIST_2);
        break;
    case IEEE80211_PARAM_CONFIG_MON_DECODER:
        if (IEEE80211_M_MONITOR != vap->iv_opmode) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Not Monitor VAP!\n");
            return -EINVAL;
        }
        /* monitor vap decoder header type: radiotap=0(default) prism=1 */
        if (value != 0 && value != 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Invalid value! radiotap=0(default) prism=1 \n");
            return -EINVAL;
        }

        if (value == 0)
            dev->type = ARPHRD_IEEE80211_RADIOTAP;
        else
            dev->type = ARPHRD_IEEE80211_PRISM;

        wlan_set_param(vap, IEEE80211_CONFIG_MON_DECODER, value);
        break;
    case IEEE80211_PARAM_SIFS_TRIGGER_RATE:
        if (ic->ic_vap_set_param) {
           retv = ic->ic_vap_set_param(vap, IEEE80211_SIFS_TRIGGER_RATE, (u_int32_t)value);
        }
        break;
    case IEEE80211_PARAM_BEACON_RATE_FOR_VAP:
        if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
            int *rate_kbps = NULL;
            int i,j,rateKbps = 0;

            switch (vap->iv_des_mode)
            {
                case IEEE80211_MODE_11B:
                    {
                        rate_table.nrates = sizeof(basic_11b_rate)/sizeof(basic_11b_rate[0]);
                        rate_table.rates = (int *)&basic_11b_rate;
                        /* Checking the boundary condition for the valid rates */
                        if ((value < 1000) || (value > 11000)) {
                            qdf_print("%s : WARNING: Please try a valid rate between 1000 to 11000 kbps.",__func__);
                            return -EINVAL;
                        }
                    }
                    break;

                case IEEE80211_MODE_TURBO_G:
                case IEEE80211_MODE_11NG_HT20:
                case IEEE80211_MODE_11NG_HT40PLUS:
                case IEEE80211_MODE_11NG_HT40MINUS:
                case IEEE80211_MODE_11NG_HT40:
                case IEEE80211_MODE_11G:
                case IEEE80211_MODE_11AXG_HE20:
                case IEEE80211_MODE_11AXG_HE40PLUS:
                case IEEE80211_MODE_11AXG_HE40MINUS:
                case IEEE80211_MODE_11AXG_HE40:
                    {
                        rate_table.nrates = sizeof(basic_11bgn_rate)/sizeof(basic_11bgn_rate[0]);
                        rate_table.rates = (int *)&basic_11bgn_rate;
                        /* Checking the boundary condition for the valid rates */
                        if ((value < 1000) || (value > 24000)){
                            qdf_print("%s : WARNING: Please try a valid rate between 1000 to 24000 kbps.",__func__);
                            return -EINVAL;
                        }
                    }
                    break;

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
                    {
                        rate_table.nrates = sizeof(basic_11na_rate)/sizeof(basic_11na_rate[0]);
                        rate_table.rates = (int *)&basic_11na_rate;
                        /* Checking the boundary condition for the valid rates */
                        if ((value < 6000) || (value > 24000)){
                            qdf_print("%s : WARNING: Please try a valid rate between 6000 to 24000 kbps.",__func__);
                            return -EINVAL;
                        }
                    }
                    break;

                default:
                    {
                        qdf_print("%s : WARNING:Invalid channel mode.",__func__);
                        return -EINVAL;
                    }
            }
            rate_kbps = rate_table.rates;

            /* Check if the rate given by user is a valid basic rate.
             * If it is valid basic rate then go with that rate or else
             * if the rate passed by the user is not valid basic rate but
             * it falls inside the valid rate boundary corresponding to the
             * phy mode, then opt for the next valid basic rate.
             */
            for (i = 0; i < rate_table.nrates; i++) {
                if (value == *rate_kbps) {
                    rateKbps = *rate_kbps;
                } else if (value < *rate_kbps) {
                    rateKbps = *rate_kbps;
                    qdf_print("%s : MSG: Not a valid basic rate.",__func__);
                    qdf_print("Since the requested rate is below 24Mbps, moving forward and selecting the next valid basic rate : %d (Kbps)",rateKbps);
                }
                if (rateKbps) {
                    /* use the iv_op_rates, the VAP may not be up yet */
                    for (j = 0; j < vap->iv_op_rates[vap->iv_des_mode].rs_nrates; j++) {
                        if (rateKbps == (((vap->iv_op_rates[vap->iv_des_mode].rs_rates[j] & IEEE80211_RATE_VAL)* 1000) / 2)) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        if (rateKbps == HIGHEST_BASIC_RATE) {
                            qdf_print("%s : MSG: Reached end of the table.",__func__);
                        } else {
                            value = *(rate_kbps+1);
                            qdf_print("%s : MSG: User opted rate is disabled. Hence going for the next rate: %d (Kbps)",__func__,value);
                        }
                    } else {
                        qdf_print("%s : Rate to be configured for beacon is: %d (Kbps)",__func__,rateKbps);
                        break;
                    }
                }
                rate_kbps++;
            }

            /* Suppose user has passed one rate and along with that rate the
             * higher rates are also not available in the rate table then
             * go with the lowest available basic rate.
             */
            if (!found) {
                uint32_t mgmt_rate;

                wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
                        WLAN_MLME_CFG_TX_MGMT_RATE, &mgmt_rate);
                rateKbps = mgmt_rate;
                qdf_print("%s: MSG: The opted rate or higher rates are not available in node rate table.",__func__);
                qdf_print("MSG: Hence choosing the lowest available rate : %d (Kbps)",rateKbps);
            }
            retv = wlan_set_param(vap, IEEE80211_BEACON_RATE_FOR_VAP,rateKbps);
            if(retv == 0)
                retv = ENETRESET;
        } else {
            qdf_print("%s : WARNING: Setting beacon rate is allowed only for AP VAP ",__func__);
            return -EINVAL;
        }
        break;
    case IEEE80211_PARAM_DISABLE_SELECTIVE_LEGACY_RATE_FOR_VAP:
        if(vap->iv_opmode == IEEE80211_M_HOSTAP) {

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
                   /* Mask has been set for rates: 24, 12 and 6 Mbps */
                    basic_valid_mask = 0x0150;
                    break;
                case IEEE80211_MODE_11B:
                   /* Mask has been set for rates: 11, 5.5, 2 and 1 Mbps */
                    basic_valid_mask = 0x000F;
                    break;
                case IEEE80211_MODE_TURBO_G:
                case IEEE80211_MODE_11NG_HT20:
                case IEEE80211_MODE_11NG_HT40PLUS:
                case IEEE80211_MODE_11NG_HT40MINUS:
                case IEEE80211_MODE_11NG_HT40:
                case IEEE80211_MODE_11AXG_HE20:
                case IEEE80211_MODE_11AXG_HE40PLUS:
                case IEEE80211_MODE_11AXG_HE40MINUS:
                case IEEE80211_MODE_11AXG_HE40:
                case IEEE80211_MODE_11G:
                   /* Mask has been set for rates: 24, 12, 6, 11, 5.5, 2 and 1 Mbps */
                    basic_valid_mask = 0x015F;
                    break;
                default:
                    break;
            }
           /*
            * Mask has been set as per the desired mode so that user will not be able to disable
            * all the supported basic rates.
            */

            if ((value & basic_valid_mask) != basic_valid_mask) {
                retv = wlan_set_param(vap, IEEE80211_CONFIG_DISABLE_SELECTIVE_LEGACY_RATE, value);
                if(retv == 0)
                    retv = ENETRESET;
            } else {
                qdf_print("%s : WARNING: Disabling all basic rates is not permitted.",__func__);
            }
        } else {
            qdf_print("%s : WARNING: This iwpriv option dis_legacy is valid only for the VAP in AP mode",__func__);
        }
        break;

    case IEEE80211_PARAM_CONFIG_NSTSCAP_WAR:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_NSTSCAP_WAR,value);
    break;

    case IEEE80211_PARAM_TXPOW_MGMT:
    {
        frame_subtype = val[1];
        if(!tx_pow_mgmt_valid(frame_subtype,&tx_power))
            return -EINVAL;
        transmit_power = (tx_power &  0xFF);
        vap->iv_txpow_mgt_frm[(frame_subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)] = transmit_power;
        if(ic->ic_is_mode_offload(ic)){
            retv = ic->ic_vap_set_param(vap, IEEE80211_CONFIG_VAP_TXPOW_MGMT, frame_subtype);
        } else {
            vap->iv_txpow_mgmt(vap,frame_subtype,transmit_power);
        }
    }
    break;
     case IEEE80211_PARAM_TXPOW:
     {
        frame_type = val[1]>>IEEE80211_SUBTYPE_TXPOW_SHIFT;
        frame_subtype = (val[1]&0xff);
        if (!tx_pow_valid(frame_type, frame_subtype, &tx_power))
            return -EINVAL;

        transmit_power = (tx_power &  0xFF);
        if (0xff == frame_subtype) {
            for (loop_index = 0; loop_index < MAX_NUM_TXPOW_MGT_ENTRY; loop_index++) {
                vap->iv_txpow_frm[frame_type>>IEEE80211_FC0_TYPE_SHIFT][loop_index] = transmit_power;
            }
        } else {
            vap->iv_txpow_frm[frame_type >> IEEE80211_FC0_TYPE_SHIFT][(frame_subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)] = transmit_power;
        }
        if (ic->ic_is_mode_offload(ic)) {
            qdf_print("TPC offload called");
            if (IEEE80211_FC0_TYPE_MGT == frame_type) {
                if (0xff != frame_subtype) {
                    if ((IEEE80211_FC0_SUBTYPE_ACTION == frame_subtype) || (IEEE80211_FC0_SUBTYPE_PROBE_REQ == frame_subtype)) {
                        retv = ic->ic_vap_set_param(vap, IEEE80211_CONFIG_VAP_TXPOW,
                                                    (frame_type << IEEE80211_FRAMETYPE_TXPOW_SHIFT) + (frame_subtype << IEEE80211_SUBTYPE_TXPOW_SHIFT) + transmit_power);
                    } else {
                        vap->iv_txpow_mgt_frm[(frame_subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)] = transmit_power;
                        retv = ic->ic_vap_set_param(vap, IEEE80211_CONFIG_VAP_TXPOW_MGMT, frame_subtype);
                    }
                } else {
                    for (loop_index=0 ; loop_index < MAX_NUM_TXPOW_MGT_ENTRY; loop_index++) {
                        vap->iv_txpow_mgt_frm[loop_index] = transmit_power;
                        retv = ic->ic_vap_set_param(vap, IEEE80211_CONFIG_VAP_TXPOW_MGMT, loop_index<<IEEE80211_FC0_SUBTYPE_SHIFT);
                    }
                    retv = ic->ic_vap_set_param(vap, IEEE80211_CONFIG_VAP_TXPOW,
                                                (frame_type << IEEE80211_FRAMETYPE_TXPOW_SHIFT) + (frame_subtype << IEEE80211_SUBTYPE_TXPOW_SHIFT) + transmit_power);
                }
            } else {
                retv = ic->ic_vap_set_param(vap, IEEE80211_CONFIG_VAP_TXPOW,
                                            (frame_type << IEEE80211_FRAMETYPE_TXPOW_SHIFT) + (frame_subtype << IEEE80211_SUBTYPE_TXPOW_SHIFT) + transmit_power);
            }
        } else {
            vap->iv_txpow(vap, frame_type, frame_subtype, transmit_power);
        }
    }
    break;
    case IEEE80211_PARAM_CHANNEL_SWITCH_MODE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_CHANNEL_SWITCH_MODE, value);
        break;

    case IEEE80211_PARAM_ENABLE_ECSA_IE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_ECSA_IE, value);
        break;

    case IEEE80211_PARAM_ECSA_OPCLASS:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_ECSA_OPCLASS, value);
        break;

    case IEEE80211_PARAM_BACKHAUL:
        retv = wlan_set_param(vap, IEEE80211_FEATURE_BACKHAUL, value);
        break;

#if DYNAMIC_BEACON_SUPPORT
    case IEEE80211_PARAM_DBEACON_EN:
        if (value > 1  ||  value < 0) {
            qdf_print("Invalid value! value should be either 0 or 1 ");
            return -EINVAL;
        }
        if (IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) {
            if (vap->iv_dbeacon != value) {
                if (value == 0) { /*  value 0 */
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "Resume beacon \n");
                    qdf_spin_lock_bh(&vap->iv_dbeacon_lock);
                    OS_CANCEL_TIMER(&vap->iv_dbeacon_suspend_beacon);
                    if (ieee80211_mlme_beacon_suspend_state(vap)) {
                        ieee80211_mlme_set_beacon_suspend_state(vap, false);
                    }
                    vap->iv_dbeacon = value;
                    qdf_spin_unlock_bh(&vap->iv_dbeacon_lock);
                } else { /*  value 1 */
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "Suspend beacon \n");
                    qdf_spin_lock_bh(&vap->iv_dbeacon_lock);
                    if (!ieee80211_mlme_beacon_suspend_state(vap)) {
                        ieee80211_mlme_set_beacon_suspend_state(vap, true);
                    }
                    vap->iv_dbeacon = value;
                    qdf_spin_unlock_bh(&vap->iv_dbeacon_lock);
                }
                wlan_deauth_all_stas(vap); /* dissociating all associated stations */
                retv = ENETRESET;
                break;
            }
            retv = EOK;
        } else {
            qdf_print("%s:%d: Dynamic beacon not allowed for vap-%d(%s)) as hidden ssid not configured. ",
                    __func__,__LINE__,vap->iv_unit,vap->iv_netdev_name);
            qdf_print("Enable hidden ssid before enable dynamic beacon ");
            return -EINVAL;
        }
        break;

    case IEEE80211_PARAM_DBEACON_RSSI_THR:
        if (value < 10  ||  value > 100) { /* min:10  max:100 */
            qdf_print("Invalid value %d. Valid values are between 10  to 100 ",value);
            return -EINVAL;
        }
        vap->iv_dbeacon_rssi_thr = (int8_t)value;
        break;

    case IEEE80211_PARAM_DBEACON_TIMEOUT:
        if (value < 30  ||  value > 3600) { /* min:30 secs  max:1hour */
            qdf_print("Invalid value %d. Valid values are between 30secs to 3600secs ",value);
            return -EINVAL;
        }
        vap->iv_dbeacon_timeout = value;
        break;

    case IEEE80211_PARAM_CONFIG_TX_CAPTURE:
        wlan_set_param(vap, IEEE80211_CONFIG_TX_CAPTURE, value);
        break;
#endif
#ifdef WLAN_SUPPORT_FILS
    case IEEE80211_PARAM_ENABLE_FILS:
        if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
          qdf_err("FILS feature cannot be enabled for Non-Transmitting MBSS VAP!");
          return -EPERM;
        }
        fils_en_period = ucfg_fd_get_enable_period(val[1], val[2]);
        retv = wlan_set_param(vap, IEEE80211_FEATURE_FILS, fils_en_period);
        break;
#endif /* WLAN_SUPPORT_FILS */
    /* 11AX TODO ( Phase II) . Check ENETRESET
     * is really needed for all HE commands
     */
    case IEEE80211_PARAM_HE_EXTENDED_RANGE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_EXTENDED_RANGE, value);
        if (retv == 0)
            retv = ENETRESET;
    break;
    case IEEE80211_PARAM_HE_DCM:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_DCM, value);
        if (retv == 0) {
            retv = ENETRESET;
        }
    break;
    case IEEE80211_PARAM_HE_FRAGMENTATION:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_FRAGMENTATION, value);
        if (retv == 0) {
            retv = ENETRESET;
        }
    break;
    case IEEE80211_PARAM_HE_MU_EDCA:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_MU_EDCA, value);
        if(retv == 0) {
            retv = ENETRESET;
        }
    break;
    case IEEE80211_PARAM_HE_SU_BFEE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_SU_BFEE, value);
        if ( retv == EOK && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_HE_SU_BFER:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_SU_BFER, value);
        if ( retv == EOK && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_HE_MU_BFEE:
        if(vap->iv_opmode == IEEE80211_M_STA) {
            retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_MU_BFEE, value);
            if ( retv == EOK && (vap->iv_novap_reset == 0)) {
                retv = ENETRESET;
            }
        } else {
            qdf_print("HE MU BFEE only supported in STA mode ");
            return -EINVAL;
        }
        break;

    case IEEE80211_PARAM_HE_MU_BFER:
        if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
            retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_MU_BFER, value);
            if ( retv == EOK && (vap->iv_novap_reset == 0)) {
                retv = ENETRESET;
            }
        } else {
            qdf_print("HE MU BFER only supported in AP mode ");
            return -EINVAL;
        }
        break;

    case IEEE80211_PARAM_HE_DL_MU_OFDMA:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_DL_MU_OFDMA, value);
        if ( retv == EOK && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_HE_UL_MU_OFDMA:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_MU_OFDMA, value);
        if (retv == EOK) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_HE_UL_MU_MIMO:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_UL_MU_MIMO, value);
        if ( retv == EOK && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
    break;

    case IEEE80211_PARAM_EXT_NSS_SUPPORT:
        if (!ic->ic_is_mode_offload(ic)) {
            return -EPERM;
        }
        retv = wlan_set_param(vap, IEEE80211_CONFIG_EXT_NSS_SUPPORT, value);
        if (retv == 0) {
            retv = ENETRESET;
        }
        break;
#if QCN_IE
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_ENABLE:
        /* Enable broadcast probe response feature only if its in HOSTAP mode
         * and hidden SSID is not enabled for this vap.
         */
        if (vap->iv_opmode == IEEE80211_M_HOSTAP && !IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) {
            if (!!value != vap->iv_bpr_enable) {
                vap->iv_bpr_enable = !!value;
                wlan_set_param(vap, IEEE80211_CONFIG_BCAST_PROBE_RESPONSE, vap->iv_bpr_enable);
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "Set the bpr_enable to %d\n", vap->iv_bpr_enable);
            }
        } else {
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                     "Invalid. Allowed only in HOSTAP mode & non-hidden SSID mode\n");
        }
        break;
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_DELAY:
        if (value < 1 || value > 255) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "Invalid value! Broadcast response delay should be between 1 and 255 \n");
            return -EINVAL;
        }
        vap->iv_bpr_delay = value;
        break;
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_LATENCY_COMPENSATION:
        if (value < 1 || value > 10) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Invalid value!"
                "Broadcast response latency compensation should be between 1 and 10 \n");
            return -EINVAL;
        }
        ic->ic_bpr_latency_comp = value;
        break;
    case IEEE80211_PARAM_BEACON_LATENCY_COMPENSATION:
        if (value < 1 || value > 10) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Invalid value!"
                "Beacon latency compensation should be between 1 and 10 \n");
            return -EINVAL;
        }
        ic->ic_bcn_latency_comp = value;
        break;
#endif
#if ATH_ACL_SOFTBLOCKING
    case IEEE80211_PARAM_SOFTBLOCK_WAIT_TIME:
        if (value >= SOFTBLOCK_WAIT_TIME_MIN && value <= SOFTBLOCK_WAIT_TIME_MAX) {
            vap->iv_softblock_wait_time = value;
        }
        else {
            qdf_print("Allowed value between (%d, %d)",SOFTBLOCK_WAIT_TIME_MIN, SOFTBLOCK_WAIT_TIME_MAX);
            retv = -EINVAL;
        }
        break;

    case IEEE80211_PARAM_SOFTBLOCK_ALLOW_TIME:
        if (value >= SOFTBLOCK_ALLOW_TIME_MIN && value <= SOFTBLOCK_ALLOW_TIME_MAX) {
            vap->iv_softblock_allow_time = value;
        }
        else {
            qdf_print("Allowed value between (%d, %d)", SOFTBLOCK_ALLOW_TIME_MIN, SOFTBLOCK_ALLOW_TIME_MAX);
            retv = -EINVAL;
        }
        break;
#endif

    case IEEE80211_PARAM_QOS_ACTION_FRAME_CONFIG:
        ic->ic_qos_acfrm_config = value;
        break;

    case IEEE80211_PARAM_HE_LTF:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_LTF, value);
        /* if the novap reset is set for debugging
         * purpose we are not resetting the VAP
         */
        if ((retv == 0) && (vap->iv_novap_reset == 0)) {
            retv = ENETRESET;
        }
        break;

    case IEEE80211_PARAM_HE_AR_GI_LTF:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_AR_GI_LTF, value);
        break;

    case IEEE80211_PARAM_HE_RTSTHRSHLD:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_RTSTHRSHLD, value);
        if (retv == 0)
            retv = ENETRESET;
        break;

    case IEEE80211_PARAM_DFS_INFO_NOTIFY_APP:
        ic->ic_dfs_info_notify_channel_available = value;
        break;
    case IEEE80211_PARAM_RSN_OVERRIDE:
        retv = wlan_set_param(vap, IEEE80211_SUPPORT_RSN_OVERRIDE, value);
        break;
    case IEEE80211_PARAM_MAP:
        retv = son_vdev_map_capability_set(vap->vdev_obj, SON_MAP_CAPABILITY, value);
        break;
    case IEEE80211_PARAM_MAP_BSS_TYPE:
        retv = son_vdev_map_capability_set(vap->vdev_obj, SON_MAP_CAPABILITY_VAP_TYPE, value);
        break;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    case IEEE80211_PARAM_NSSOL_VAP_INSPECT_MODE:
        if (osifp->nss_wifiol_ctx && ic->nss_vops) {
            ic->nss_vops->ic_osif_nss_vdev_set_inspection_mode(osifp, (uint32_t)value);
        }
        break;
#endif

    case IEEE80211_PARAM_DISABLE_CABQ :
        retv = wlan_set_param(vap, IEEE80211_FEATURE_DISABLE_CABQ, value);
        break;

    case IEEE80211_PARAM_CSL_SUPPORT:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_CSL_SUPPORT, value);
        break;

    case IEEE80211_PARAM_TIMEOUTIE:
        retv = wlan_set_param(vap, IEEE80211_SUPPORT_TIMEOUTIE, value);
        break;

    case IEEE80211_PARAM_PMF_ASSOC:
        retv = wlan_set_param(vap, IEEE80211_SUPPORT_PMF_ASSOC, value);
        break;

    case IEEE80211_PARAM_BEST_UL_HYST:
        ucfg_son_set_bestul_hyst(vap->vdev_obj, value);
        break;

    case IEEE80211_PARAM_HE_TX_MCSMAP:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_TX_MCSMAP, value);
        if (retv == 0)
            retv = ENETRESET;
        break;

    case IEEE80211_PARAM_HE_RX_MCSMAP:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_RX_MCSMAP, value);
        if (retv == 0)
            retv = ENETRESET;
        break;

    case IEEE80211_PARAM_CONFIG_M_COPY:
        wlan_set_param(vap, IEEE80211_CONFIG_M_COPY, value);

    case IEEE80211_PARAM_NSSOL_VAP_READ_RXPREHDR:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_READ_RXPREHDR, value);
        break;

    case IEEE80211_PARAM_CONFIG_CAPTURE_LATENCY_ENABLE:
        wlan_set_param(vap, IEEE80211_CONFIG_CAPTURE_LATENCY_ENABLE, value);
        break;

    case IEEE80211_PARAM_BA_BUFFER_SIZE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_BA_BUFFER_SIZE, value);
        break;

    case IEEE80211_PARAM_HE_SOUNDING_MODE:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_SOUNDING_MODE, value);
        break;

    case IEEE80211_PARAM_HE_HT_CTRL:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_HT_CTRL, value);
        if (retv == 0) {
            retv = ENETRESET;
        }
        break;
    case IEEE80211_PARAM_WIFI_DOWN_IND:
         retv = wlan_set_param(vap, IEEE80211_CONFIG_WIFI_DOWN_IND, value);
         break;

    case IEEE80211_PARAM_LOG_ENABLE_BSTEERING_RSSI:
        if(value == 0 || value == 1)
        {
            son_record_inst_rssi_log_enable(vap->vdev_obj, value);
        }
        else
            qdf_err("Incorrect value for bsteerrssi_log \n");
        break;

    case IEEE80211_PARAM_FT_ENABLE:
        if (!ic->ic_cfg80211_config) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "FT supported only with cfg80211\n");
            return -EINVAL;
        }

        if(vap->iv_opmode == IEEE80211_M_STA) {
            if (value > 1  ||  value < 0) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                    "Invalid value! value should be either 0 or 1\n");
                return -EINVAL;
            }
            retv = wlan_set_param(vap, IEEE80211_CONFIG_FT_ENABLE, value);
            if (retv == 0) {
                retv = ENETRESET;
            }
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "Valid only in STA mode\n");
            return -EINVAL;
        }
        break;
#if WLAN_SER_DEBUG
    case IEEE80211_PARAM_WLAN_SER_HISTORY:
        wlan_ser_print_history(vap->vdev_obj, value, val[2]);
        break;
#endif

    case IEEE80211_PARAM_BCN_STATS_RESET:
        if(vap->reset_bcn_stats)
            vap->reset_bcn_stats(vap);
        break;
#if WLAN_SER_UTF
    case IEEE80211_PARAM_WLAN_SER_TEST:
        wlan_ser_utf_main(vap->vdev_obj, value, val[2]);
        break;
#endif
#if QCA_SUPPORT_SON
     case IEEE80211_PARAM_SON_EVENT_BCAST:
         son_core_enable_disable_vdev_bcast_events(vap->vdev_obj, !!value);
         break;
     case IEEE80211_PARAM_WHC_BACKHAUL_TYPE:
         if(!son_set_backhaul_type_mixedbh(vap->vdev_obj, value)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
               "Error, in setting backhaul type and sonmode");
         } else {
             son_update_bss_ie(vap->vdev_obj);
             wlan_pdev_beacon_update(ic);
         }
         break;
     case IEEE80211_PARAM_WHC_MIXEDBH_ULRATE:
         if(!son_set_ul_mixedbh(vap->vdev_obj, value)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "Error, in setting uplink rate");
         }
         break;
#endif
    case IEEE80211_PARAM_RAWMODE_OPEN_WAR:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_RAWMODE_OPEN_WAR, !!value);
        break;
#if UMAC_SUPPORT_WPA3_STA
    case IEEE80211_PARAM_EXTERNAL_AUTH_STATUS:
        qdf_timer_sync_cancel(&vap->iv_sta_external_auth_timer);
        vap->iv_mlme_priv->im_request_type = 0;
        osifp->app_filter &= ~(IEEE80211_FILTER_TYPE_AUTH);
        IEEE80211_DELIVER_EVENT_MLME_AUTH_COMPLETE(vap, NULL, value);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "external auth status %d",value);
        break;
    case IEEE80211_PARAM_SAE_AUTH_ATTEMPTS:
        vap->iv_sae_max_auth_attempts = !!value;
        retv = 0;
        break;
#endif
    case IEEE80211_PARAM_DPP_VAP_MODE:
        vap->iv_dpp_vap_mode = !!value;
        retv = 0;
        break;
    case IEEE80211_PARAM_HE_BSR_SUPPORT:
        retv = wlan_set_param(vap, IEEE80211_CONFIG_HE_BSR_SUPPORT, value);
        if((retv == 0) && (vap->iv_novap_reset == 0)){
            retv = ENETRESET;
        }
    break;
    case IEEE80211_PARAM_MAP_VAP_BEACONING:
        retv = son_vdev_map_capability_set(vap->vdev_obj,
                            SON_MAP_CAPABILITY_VAP_UP, value);
        break;
    case IEEE80211_PARAM_UNIFORM_RSSI:
        ic->ic_uniform_rssi = !!value;
        retv = 0;
        break;
    case IEEE80211_PARAM_CSA_INTEROP_PHY:
        vap->iv_csa_interop_phy = !!value;
        retv = 0;
        break;
    case IEEE80211_PARAM_CSA_INTEROP_BSS:
        vap->iv_csa_interop_bss = !!value;
        retv = 0;
        break;
    case IEEE80211_PARAM_CSA_INTEROP_AUTH:
        vap->iv_csa_interop_auth = !!value;
        retv = 0;
        break;
    }
    if (retv == ENETRESET)
    {
        retv = IS_UP(dev) ? osif_vap_init(dev, RESCAN) : 0;
    }
    return retv;

}

static char num_to_char(u_int8_t n)
{
    if ( n >= 10 && n <= 15 )
        return n  - 10 + 'A';
    if ( n >= 0 && n <= 9 )
        return n  + '0';
    return ' '; //Blank space
}

/* convert MAC address in array format to string format
 * inputs:
 *     addr - mac address array
 * output:
 *     macstr - MAC address string format */
static void macaddr_num_to_str(u_int8_t *addr, char *macstr)
{
    int i, j=0;

    for ( i = 0; i < IEEE80211_ADDR_LEN; i++ ) {
        macstr[j++] = num_to_char(addr[i] >> 4);
        macstr[j++] = num_to_char(addr[i] & 0xF);
    }
}

int ieee80211_ucfg_getparam(wlan_if_t vap, int param, int *value)
{
    osif_dev  *osifp = (osif_dev *)wlan_vap_get_registered_handle(vap);
    wlan_dev_t ic = wlan_vap_get_devhandle(vap);
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj);
    ol_txrx_vdev_handle vdev_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
    char *extra = (char *)value;
    int retv = 0;
    int *txpow_frm_subtype = value;
    u_int8_t frame_subtype;
    u_int8_t frame_type;
#if ATH_SUPPORT_DFS
    int tmp;
#endif
    struct wlan_objmgr_pdev *pdev;
#if WLAN_SUPPORT_SPLITMAC || defined(ATH_EXT_AP)
    struct wlan_objmgr_vdev *vdev = vap->vdev_obj;
#endif

	if (!osifp || osifp->is_delete_in_progress)
		return -EINVAL;

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is null", __func__);
        return -1;
    }
    switch (param)
    {
    case IEEE80211_PARAM_MAXSTA:
        qdf_print("Getting Max Stations: %d", vap->iv_max_aid - 1);
        *value = vap->iv_max_aid - 1;
        break;
    case IEEE80211_PARAM_AUTO_ASSOC:
        *value = wlan_get_param(vap, IEEE80211_AUTO_ASSOC);
        break;
    case IEEE80211_PARAM_VAP_COUNTRY_IE:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_COUNTRY_IE);
        break;
    case IEEE80211_PARAM_VAP_DOTH:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_DOTH);
        break;
    case IEEE80211_PARAM_HT40_INTOLERANT:
        *value = wlan_get_param(vap, IEEE80211_HT40_INTOLERANT);
        break;

    case IEEE80211_PARAM_CHWIDTH:
        *value = wlan_get_param(vap, IEEE80211_CHWIDTH);
        break;

    case IEEE80211_PARAM_CHEXTOFFSET:
        *value = wlan_get_param(vap, IEEE80211_CHEXTOFFSET);
        break;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
    case IEEE80211_PARAM_STA_QUICKKICKOUT:
        *value = wlan_get_param(vap, IEEE80211_STA_QUICKKICKOUT);
        break;
#endif
    case IEEE80211_PARAM_CHSCANINIT:
        *value = wlan_get_param(vap, IEEE80211_CHSCANINIT);
        break;

    case IEEE80211_PARAM_COEXT_DISABLE:
        *value = ((ic->ic_flags & IEEE80211_F_COEXT_DISABLE) != 0);
        break;

    case IEEE80211_PARAM_NR_SHARE_RADIO_FLAG:
        *value = ic->ic_nr_share_radio_flag;
        break;

    case IEEE80211_PARAM_AUTHMODE:
        //fixme how it used to be done: *value = osifp->authmode;
        {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
            uint32_t authmode;
            authmode = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE);
            *value = 0;
            if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_WPA) | (1 << WLAN_CRYPTO_AUTH_RSNA)))
                *value = WLAN_CRYPTO_AUTH_WPA;
            else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_OPEN) | (1 << WLAN_CRYPTO_AUTH_SHARED)))
               *value = WLAN_CRYPTO_AUTH_AUTO;
            else {
                if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_OPEN)))
                   *value = WLAN_CRYPTO_AUTH_OPEN;
                else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_AUTO)))
                   *value = WLAN_CRYPTO_AUTH_AUTO;
                else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_NONE)))
                   *value = WLAN_CRYPTO_AUTH_NONE;
                else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_SHARED)))
                   *value = WLAN_CRYPTO_AUTH_SHARED;
                else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_WPA)))
                   *value = WLAN_CRYPTO_AUTH_WPA;
                else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_RSNA)))
                   *value = WLAN_CRYPTO_AUTH_RSNA;
                else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_8021X)))
                   *value = WLAN_CRYPTO_AUTH_8021X;
                else if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_CCKM)))
                   *value = WLAN_CRYPTO_AUTH_CCKM;
                else if (authmode & (uint32_t)(1 << WLAN_CRYPTO_AUTH_SAE))
                   *value = WLAN_CRYPTO_AUTH_SAE;
            }
#else
            ieee80211_auth_mode modes[IEEE80211_AUTH_MAX];
            retv = wlan_get_auth_modes(vap, modes, IEEE80211_AUTH_MAX);
            if (retv > 0)
            {
                *value = modes[0];
                if((retv > 1) && (modes[0] == IEEE80211_AUTH_OPEN) && (modes[1] == IEEE80211_AUTH_SHARED))
                    *value =  IEEE80211_AUTH_AUTO;
                retv = 0;
            }
#endif
        }
        break;
     case IEEE80211_PARAM_BANDWIDTH:
         {
           *value=ieee80211_ucfg_get_bandwidth(vap);
           break;
         }
     case IEEE80211_PARAM_FREQ_BAND:
         {
           *value=ieee80211_ucfg_get_band(vap);
           break;
         }
     case IEEE80211_PARAM_EXTCHAN:
        {
           *value=ieee80211_ucfg_get_extchan(vap);
           break;
        }
     case IEEE80211_PARAM_SECOND_CENTER_FREQ:
        {
            if ((vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80_80) ||
                    (vap->iv_des_mode == IEEE80211_MODE_11AXA_HE80_80)) {
                *value= ieee80211_ieee2mhz(ic,vap->iv_bsschan->ic_vhtop_ch_freq_seg2,IEEE80211_CHAN_5GHZ);
            }
            else {
                *value = 0;
                qdf_print(" center freq not present ");
            }
        }
        break;

     case IEEE80211_PARAM_ATH_SUPPORT_VLAN:
        {
            *value = vap->vlan_set_flags;   /* dev->flags to control VLAN tagged packets sent by NW stack */
            break;
        }

     case IEEE80211_DISABLE_BCN_BW_NSS_MAP:
        {
            *value = ic->ic_disable_bcn_bwnss_map;
            break;
        }
     case IEEE80211_DISABLE_STA_BWNSS_ADV:
        {
            *value = ic->ic_disable_bwnss_adv;
            break;
        }
     case IEEE80211_PARAM_MCS:
        {
            *value=-1;  /* auto rate */
            break;
        }
    case IEEE80211_PARAM_MCASTCIPHER:
        {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        int mcastcipher;
        mcastcipher = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_MCAST_CIPHER);
            if (mcastcipher & 1<<IEEE80211_CIPHER_WEP)
                *value = IEEE80211_CIPHER_WEP;
            if (mcastcipher & 1<<IEEE80211_CIPHER_TKIP)
                *value = IEEE80211_CIPHER_TKIP;
            if (mcastcipher & 1<<IEEE80211_CIPHER_AES_CCM)
                *value = IEEE80211_CIPHER_AES_CCM;
            if (mcastcipher & 1<<IEEE80211_CIPHER_AES_CCM_256)
                *value = IEEE80211_CIPHER_AES_CCM_256;
            if (mcastcipher & 1<<IEEE80211_CIPHER_AES_GCM)
                *value = IEEE80211_CIPHER_AES_GCM;
            if (mcastcipher & 1<<IEEE80211_CIPHER_AES_GCM_256)
                *value = IEEE80211_CIPHER_AES_GCM_256;
            if (mcastcipher & 1<<IEEE80211_CIPHER_CKIP)
                *value = IEEE80211_CIPHER_CKIP;
#if ATH_SUPPORT_WAPI
            if (mcastcipher & 1<<IEEE80211_CIPHER_WAPI)
                *value = IEEE80211_CIPHER_WAPI;
#endif
            if (mcastcipher & 1<<IEEE80211_CIPHER_NONE)
                *value = IEEE80211_CIPHER_NONE;
#else
            ieee80211_cipher_type mciphers[1];
            int count;
            count = wlan_get_mcast_ciphers(vap,mciphers,1);
            if (count == 1)
                *value = mciphers[0];
#endif
        }
        break;
    case IEEE80211_PARAM_MCASTKEYLEN:
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        qdf_print("Not supported in crypto convergence");
	*value = 0;
#else
        *value = wlan_get_rsn_cipher_param(vap, IEEE80211_MCAST_CIPHER_LEN);
#endif
        break;
    case IEEE80211_PARAM_UCASTCIPHERS:
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        *value = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
#else
        do {
            ieee80211_cipher_type uciphers[IEEE80211_CIPHER_MAX];
            int i, count;
            count = wlan_get_ucast_ciphers(vap, uciphers, IEEE80211_CIPHER_MAX);
            *value = 0;
            for (i = 0; i < count; i++) {
                *value |= 1<<uciphers[i];
            }
    } while (0);
#endif
        break;
    case IEEE80211_PARAM_UCASTCIPHER:
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        *value = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
#else
        do {
            ieee80211_cipher_type uciphers[1];
            int count = 0;
            count = wlan_get_ucast_ciphers(vap, uciphers, 1);
            *value = 0;
            if (count == 1)
                *value |= 1<<uciphers[0];
        } while (0);
#endif
        break;
    case IEEE80211_PARAM_UCASTKEYLEN:
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        qdf_print("Not supported in crypto convergence");
	*value = 0;
#else
        *value = wlan_get_rsn_cipher_param(vap, IEEE80211_UCAST_CIPHER_LEN);
#endif
        break;
    case IEEE80211_PARAM_PRIVACY:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_PRIVACY);
        break;
    case IEEE80211_PARAM_COUNTERMEASURES:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_COUNTER_MEASURES);
        break;
    case IEEE80211_PARAM_HIDESSID:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_HIDE_SSID);
        break;
    case IEEE80211_PARAM_APBRIDGE:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_APBRIDGE);
        break;
    case IEEE80211_PARAM_KEYMGTALGS:
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        *value = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_KEY_MGMT);
#else
        *value = wlan_get_rsn_cipher_param(vap, IEEE80211_KEYMGT_ALGS);
#endif
        break;
    case IEEE80211_PARAM_RSNCAPS:
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        *value = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_RSN_CAP);
#else
        *value = wlan_get_rsn_cipher_param(vap, IEEE80211_RSN_CAPS);
#endif
        break;
    case IEEE80211_PARAM_WPA:
        {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
            uint32_t authmode;
            authmode = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE);
            *value = 0;
            if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_WPA)))
               *value |= 0x1;
            if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_RSNA)))
               *value |= 0x2;
#else
            ieee80211_auth_mode modes[IEEE80211_AUTH_MAX];
            int count, i;
            *value = 0;
            count = wlan_get_auth_modes(vap,modes,IEEE80211_AUTH_MAX);
            for (i = 0; i < count; i++) {
                if (modes[i] == IEEE80211_AUTH_WPA)
                    *value |= 0x1;
                if (modes[i] == IEEE80211_AUTH_RSNA)
                    *value |= 0x2;
            }
#endif
        }
        break;
#if DBG_LVL_MAC_FILTERING
    case IEEE80211_PARAM_DBG_LVL_MAC:
        *value = vap->iv_print.dbgLVLmac_on;
        break;
#endif

    case IEEE80211_PARAM_DUMP_RA_TABLE:
         wlan_show_me_ra_table(vap);
         break;

    case IEEE80211_PARAM_CONFIG_CATEGORY_VERBOSE:
         *value = wlan_show_shared_print_ctrl_category_verbose_table();
         break;

#ifdef QCA_OL_DMS_WAR
    case IEEE80211_PARAM_DMS_AMSDU_WAR:
         *value = vap->dms_amsdu_war;
         break;
#endif

    case IEEE80211_PARAM_DBG_LVL:
        {
            char c[128];
            *value = (u_int32_t)wlan_get_debug_flags(vap);
            snprintf(c, sizeof(c), "0x%x", *value);
            strlcpy(extra, c, sizeof(c));
        }
        break;
    case IEEE80211_PARAM_DBG_LVL_HIGH:
        /* no need to show IEEE80211_MSG_ANY to user */
        *value = (u_int32_t)((wlan_get_debug_flags(vap) & 0x7fffffff00000000ULL) >> 32);
        break;
    case IEEE80211_PARAM_MIXED_MODE:
        *value = vap->mixed_encryption_mode;
        break;
#if UMAC_SUPPORT_IBSS
    case IEEE80211_PARAM_IBSS_CREATE_DISABLE:
        *value = osifp->disable_ibss_create;
        break;
#endif
	case IEEE80211_PARAM_WEATHER_RADAR_CHANNEL:
        *value = wlan_get_param(vap, IEEE80211_WEATHER_RADAR);
        break;
    case IEEE80211_PARAM_SEND_DEAUTH:
        *value = wlan_get_param(vap, IEEE80211_SEND_DEAUTH);
        break;
    case IEEE80211_PARAM_WEP_KEYCACHE:
        *value = wlan_get_param(vap, IEEE80211_WEP_KEYCACHE);
	break;
	case IEEE80211_PARAM_GET_ACS:
        *value = wlan_get_param(vap,IEEE80211_GET_ACS_STATE);
    break;
	case IEEE80211_PARAM_GET_CAC:
        *value = wlan_get_param(vap,IEEE80211_GET_CAC_STATE);
	break;
    case IEEE80211_PARAM_SIFS_TRIGGER:
        if (!ic->ic_is_mode_offload(ic))
            return -EPERM;
        *value = vap->iv_sifs_trigger_time;
        break;
    case IEEE80211_PARAM_BEACON_INTERVAL:
        *value = wlan_get_param(vap, IEEE80211_BEACON_INTVAL);
        break;
#if ATH_SUPPORT_AP_WDS_COMBO
    case IEEE80211_PARAM_NO_BEACON:
        *value = wlan_get_param(vap, IEEE80211_NO_BEACON);
        break;
#endif
    case IEEE80211_PARAM_PUREG:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_PUREG);
        break;
    case IEEE80211_PARAM_PUREN:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_PURE11N);
        break;
    case IEEE80211_PARAM_PURE11AC:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_PURE11AC);
        break;
    case IEEE80211_PARAM_STRICT_BW:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_STRICT_BW);
        break;
    case IEEE80211_PARAM_WDS:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_WDS);
        break;
    case IEEE80211_PARAM_DA_WAR_ENABLE:
        *value = cdp_txrx_get_vdev_param(wlan_psoc_get_dp_handle(psoc),
                 (struct cdp_vdev *)vdev_txrx_handle, CDP_ENABLE_DA_WAR);
        break;
#if WDS_VENDOR_EXTENSION
    case IEEE80211_PARAM_WDS_RX_POLICY:
        *value = wlan_get_param(vap, IEEE80211_WDS_RX_POLICY);
        break;
#endif
#if WLAN_SUPPORT_GREEN_AP
    case IEEE80211_IOCTL_GREEN_AP_PS_ENABLE:
        {
        uint8_t ps_enable;
        ucfg_green_ap_get_ps_config(pdev, &ps_enable);
        *value = ps_enable;
        }
        break;
    case IEEE80211_IOCTL_GREEN_AP_PS_TIMEOUT:
        {
        uint32_t trans_time;
        ucfg_green_ap_get_transition_time(pdev, &trans_time);
        *value = trans_time;
        }
        break;
    case IEEE80211_IOCTL_GREEN_AP_ENABLE_PRINT:
        *value = ucfg_green_ap_get_debug_prints(pdev);
        break;
#endif
    case IEEE80211_PARAM_WPS:
        *value = wlan_get_param(vap, IEEE80211_WPS_MODE);
        break;
    case IEEE80211_PARAM_EXTAP:
        *value = dp_is_extap_enabled(vdev);
        break;


    case IEEE80211_PARAM_STA_FORWARD:
    *value  = wlan_get_param(vap, IEEE80211_FEATURE_STAFWD);
    break;

    case IEEE80211_PARAM_DYN_BW_RTS:
        *value = wlan_get_param(vap, IEEE80211_DYN_BW_RTS);
        break;

    case IEEE80211_PARAM_CWM_EXTPROTMODE:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_EXTPROTMODE);
        break;
    case IEEE80211_PARAM_CWM_EXTPROTSPACING:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_EXTPROTSPACING);
        break;
    case IEEE80211_PARAM_CWM_ENABLE:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_ENABLE);
        break;
    case IEEE80211_PARAM_CWM_EXTBUSYTHRESHOLD:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_CWM_EXTBUSYTHRESHOLD);
        break;
    case IEEE80211_PARAM_DOTH:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_DOTH);
        break;
    case IEEE80211_PARAM_WMM:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_WMM);
        break;
    case IEEE80211_PARAM_PROTMODE:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_PROTECTION_MODE);
        break;
    case IEEE80211_PARAM_DRIVER_CAPS:
        *value = wlan_get_param(vap, IEEE80211_DRIVER_CAPS);
        break;
    case IEEE80211_PARAM_MACCMD:
        *value = wlan_get_acl_policy(vap, IEEE80211_ACL_FLAG_ACL_LIST_1);
        break;
    case IEEE80211_PARAM_DROPUNENCRYPTED:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_DROP_UNENC);
    break;
    case IEEE80211_PARAM_DTIM_PERIOD:
        *value = wlan_get_param(vap, IEEE80211_DTIM_INTVAL);
        break;
    case IEEE80211_PARAM_SHORT_GI:
        *value = wlan_get_param(vap, IEEE80211_SHORT_GI);
        break;
   case IEEE80211_PARAM_SHORTPREAMBLE:
        *value = wlan_get_param(vap, IEEE80211_SHORT_PREAMBLE);
        break;
   case IEEE80211_PARAM_CHAN_NOISE:
        *value = vap->iv_ic->ic_get_cur_chan_nf(vap->iv_ic);
        break;
   case IEEE80211_PARAM_STA_MAX_CH_CAP:
        *value = vap->iv_sta_max_ch_cap;
        break;
    case IEEE80211_PARAM_OBSS_NB_RU_TOLERANCE_TIME:
        *value = ic->ic_obss_nb_ru_tolerance_time;
        break;
    /*
    * Support to Mcast Enhancement
    */
#if ATH_SUPPORT_IQUE
    case IEEE80211_PARAM_ME:
        *value = wlan_get_param(vap, IEEE80211_ME);
        break;
#if ATH_SUPPORT_ME_SNOOP_TABLE
    case IEEE80211_PARAM_MEDUMP:
        *value = wlan_get_param(vap, IEEE80211_MEDUMP);
        break;
    case IEEE80211_PARAM_MEDEBUG:
        *value = wlan_get_param(vap, IEEE80211_MEDEBUG);
        break;
    case IEEE80211_PARAM_ME_SNOOPLENGTH:
        *value = wlan_get_param(vap, IEEE80211_ME_SNOOPLENGTH);
        break;
    case IEEE80211_PARAM_ME_TIMEOUT:
        *value = wlan_get_param(vap, IEEE80211_ME_TIMEOUT);
        break;
    case IEEE80211_PARAM_ME_DROPMCAST:
        *value = wlan_get_param(vap, IEEE80211_ME_DROPMCAST);
        break;
    case IEEE80211_PARAM_ME_SHOWDENY:
        *value = wlan_get_param(vap, IEEE80211_ME_SHOWDENY);
        break;
#endif
    case IEEE80211_PARAM_HBR_TIMER:
        *value = wlan_get_param(vap, IEEE80211_HBR_TIMER);
        break;
    case IEEE80211_PARAM_HBR_STATE:
        wlan_get_hbrstate(vap);
        *value = 0;
        break;
    case IEEE80211_PARAM_GETIQUECONFIG:
        *value = wlan_get_param(vap, IEEE80211_IQUE_CONFIG);
        break;
#endif /*ATH_SUPPORT_IQUE*/

    case IEEE80211_PARAM_SCANVALID:
        *value = 0;
        if (osifp->os_opmode == IEEE80211_M_STA ||
                osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
            *value = wlan_connection_sm_get_param(osifp->sm_handle,
                                                    WLAN_CONNECTION_PARAM_SCAN_CACHE_VALID_TIME);
        }
        break;
    case IEEE80211_PARAM_COUNTRYCODE:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_COUNTRYCODE);
        break;
    case IEEE80211_PARAM_11N_RATE:
        *value = wlan_get_param(vap, IEEE80211_FIXED_RATE);
        qdf_print("Getting Rate Series: %x",*value);
        break;
    case IEEE80211_PARAM_VHT_MCS:
        *value = wlan_get_param(vap, IEEE80211_FIXED_VHT_MCS);
        qdf_print("Getting VHT Rate set: %x",*value);
        break;
    case IEEE80211_PARAM_HE_MCS:
        *value = wlan_get_param(vap, IEEE80211_FIXED_HE_MCS);
        qdf_print("Getting HE MCS Rate set: %x",*value);
        break;
    case IEEE80211_PARAM_NSS:
        if(!ic->ic_is_mode_offload(ic))
            return -EPERM;
        *value = wlan_get_param(vap, IEEE80211_FIXED_NSS);
        qdf_print("Getting Nss: %x",*value);
        break;
    case IEEE80211_PARAM_HE_UL_SHORTGI:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_SHORTGI);
        qdf_info("Getting UL Short GI: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_UL_LTF:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_LTF);
        qdf_info("Getting UL HE LTF: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_UL_NSS:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_NSS);
        qdf_info("Getting UL HE NSS: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_UL_PPDU_BW:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_PPDU_BW);
        qdf_info("Getting UL HE PPDU BW: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_UL_LDPC:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_LDPC);
        qdf_info("Getting UL HE LDPC: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_UL_STBC:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_STBC);
        qdf_info("Getting UL HE STBC: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_UL_FIXED_RATE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_FIXED_RATE);
        qdf_info("Getting UL HE MCS: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_AMSDU_IN_AMPDU_SUPRT:
        *value = vap->iv_he_amsdu_in_ampdu_suprt;
        qdf_info("Getting HE AMSDU in AMPDU support: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_SUBFEE_STS_SUPRT:
    {
        char c[128];
        *value       = vap->iv_he_subfee_sts_lteq80;
        *(value + 1) = vap->iv_he_subfee_sts_gt80;

        qdf_info("Getting HE SUBFEE STS support lteq80: %d gt80: %d\n", *value, *(value + 1));

        snprintf(c, sizeof(c), "0x%x 0x%x", *value, *(value + 1));
        strlcpy(extra, c, sizeof(c));
    }
    break;

    case IEEE80211_PARAM_HE_4XLTF_800NS_GI_RX_SUPRT:
        *value = vap->iv_he_4xltf_800ns_gi;
        qdf_info("Getting HE 4xltf+800ns support: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_1XLTF_800NS_GI_RX_SUPRT:
        *value = vap->iv_he_1xltf_800ns_gi;
        qdf_info("Getting HE 1xltf+800ns support: %d\n", *value);
    break;

    case IEEE80211_PARAM_HE_MAX_NC_SUPRT:
        *value = vap->iv_he_max_nc;
        qdf_info("Getting HE max_nc support: %d\n", *value);
    break;

    case IEEE80211_PARAM_TWT_RESPONDER_SUPRT:
        *value = vap->iv_twt_rsp;
        qdf_info("Getting twt responder support: %d\n", *value);
    break;

    case IEEE80211_PARAM_STA_COUNT:
        {
            int sta_count =0;
            if(osifp->os_opmode == IEEE80211_M_STA)
                return -EINVAL;

            sta_count = wlan_iterate_station_list(vap, NULL,NULL);
            *value = sta_count;
            break;
        }
    case IEEE80211_PARAM_NO_VAP_RESET:
        *value = vap->iv_novap_reset;
        qdf_print("Getting VAP reset: %x",*value);
        break;

    case IEEE80211_PARAM_VHT_SGIMASK:
        *value = wlan_get_param(vap, IEEE80211_VHT_SGIMASK);
        qdf_print("Getting VHT SGI MASK: %x",*value);
        break;

    case IEEE80211_PARAM_VHT80_RATEMASK:
        *value = wlan_get_param(vap, IEEE80211_VHT80_RATEMASK);
        qdf_print("Getting VHT80 RATE MASK: %x",*value);
        break;

    case IEEE80211_PARAM_OPMODE_NOTIFY:
        *value = wlan_get_param(vap, IEEE80211_OPMODE_NOTIFY_ENABLE);
        qdf_print("Getting Notify element status: %x",*value);
        break;

    case IEEE80211_PARAM_LDPC:
        *value = wlan_get_param(vap, IEEE80211_SUPPORT_LDPC);
        qdf_print("Getting LDPC: %x",*value);
        break;
    case IEEE80211_PARAM_TX_STBC:
        *value = wlan_get_param(vap, IEEE80211_SUPPORT_TX_STBC);
        qdf_print("Getting TX STBC: %x",*value);
        break;
    case IEEE80211_PARAM_RX_STBC:
        *value = wlan_get_param(vap, IEEE80211_SUPPORT_RX_STBC);
        qdf_print("Getting RX STBC: %x",*value);
        break;
    case IEEE80211_PARAM_VHT_TX_MCSMAP:
        *value = wlan_get_param(vap, IEEE80211_VHT_TX_MCSMAP);
        qdf_print("Getting VHT TX MCS MAP set: %x",*value);
        break;
    case IEEE80211_PARAM_VHT_RX_MCSMAP:
        *value = wlan_get_param(vap, IEEE80211_VHT_RX_MCSMAP);
        qdf_print("Getting VHT RX MCS MAP set: %x",*value);
        break;
    case IEEE80211_PARAM_11N_RETRIES:
        *value = wlan_get_param(vap, IEEE80211_FIXED_RETRIES);
        qdf_print("Getting Retry Series: %x",*value);
        break;
    case IEEE80211_PARAM_MCAST_RATE:
        *value = wlan_get_param(vap, IEEE80211_MCAST_RATE);
        break;
    case IEEE80211_PARAM_BCAST_RATE:
        *value = wlan_get_param(vap, IEEE80211_BCAST_RATE);
        break;
    case IEEE80211_PARAM_CCMPSW_ENCDEC:
        *value = vap->iv_ccmpsw_seldec;
        break;
    case IEEE80211_PARAM_UAPSDINFO:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_UAPSD);
        break;
    case IEEE80211_PARAM_STA_PWR_SET_PSPOLL:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_PSPOLL);
        break;
    case IEEE80211_PARAM_NETWORK_SLEEP:
        *value= (u_int32_t)wlan_get_powersave(vap);
        break;
#if UMAC_SUPPORT_WNM
    case IEEE80211_PARAM_WNM_SLEEP:
        *value= (u_int32_t)wlan_get_powersave(vap);
        break;
#endif
#if UMAC_SUPPORT_QBSSLOAD
    case IEEE80211_PARAM_QBSS_LOAD:
        *value = wlan_get_param(vap, IEEE80211_QBSS_LOAD);
	break;
#if ATH_SUPPORT_HS20
    case IEEE80211_PARAM_HC_BSSLOAD:
        *value = vap->iv_hc_bssload;
        break;
#endif /* ATH_SUPPORT_HS20 */
#endif /* UMAC_SUPPORT_QBSSLOAD */
#if UMAC_SUPPORT_XBSSLOAD
    case IEEE80211_PARAM_XBSS_LOAD:
        *value = wlan_get_param(vap, IEEE80211_XBSS_LOAD);
        qdf_info("Extended BSS Load: %s\n",*value? "Enabled":"Disabled");
        if (*value) {
            qdf_info("MU-MIMO capable STA Count: %d",
                     vap->iv_mu_bformee_sta_assoc);
#ifdef QCA_SUPPORT_CP_STATS
            qdf_info("Spatial Stream Underutilization: %d",
                     pdev_chan_stats_ss_under_util_get(ic->ic_pdev_obj));
            qdf_info("Observable secondary 20MHz Utilization: %d",
                     pdev_chan_stats_sec_20_util_get(ic->ic_pdev_obj));
            qdf_info("Observable secondary 40MHz Utilization: %d",
                     pdev_chan_stats_sec_40_util_get(ic->ic_pdev_obj));
            qdf_info("Observable secondary 80MHz Utilization: %d",
                     pdev_chan_stats_sec_80_util_get(ic->ic_pdev_obj));
#endif
        }
        break;
#endif /* UMAC_SUPPORT_XBSSLOAD */
#if UMAC_SUPPORT_CHANUTIL_MEASUREMENT
    case IEEE80211_PARAM_CHAN_UTIL_ENAB:
        *value = wlan_get_param(vap, IEEE80211_CHAN_UTIL_ENAB);
        break;
    case IEEE80211_PARAM_CHAN_UTIL:
        *value = wlan_get_param(vap, IEEE80211_CHAN_UTIL);
        break;
#endif /* UMAC_SUPPORT_CHANUTIL_MEASUREMENT */
#if UMAC_SUPPORT_QUIET
    case IEEE80211_PARAM_QUIET_PERIOD:
        *value = wlan_quiet_get_param(vap);
        break;
#endif /* UMAC_SUPPORT_QUIET */
    case IEEE80211_PARAM_GET_OPMODE:
        *value = wlan_get_param(vap, IEEE80211_GET_OPMODE);
        break;
    case IEEE80211_PARAM_MBO:
        *value = wlan_get_param(vap, IEEE80211_MBO);
        break;
    case IEEE80211_PARAM_MBO_CAP:
        *value = wlan_get_param(vap, IEEE80211_MBOCAP);
        break;
    case IEEE80211_PARAM_MBO_ASSOC_DISALLOW:
        *value = wlan_get_param(vap,IEEE80211_MBO_ASSOC_DISALLOW);
        break;
    case IEEE80211_PARAM_MBO_CELLULAR_PREFERENCE:
        *value = wlan_get_param(vap,IEEE80211_MBO_CELLULAR_PREFERENCE);
        break;
    case IEEE80211_PARAM_MBO_TRANSITION_REASON:
        *value = wlan_get_param(vap,IEEE80211_MBO_TRANSITION_REASON);
        break;
    case IEEE80211_PARAM_MBO_ASSOC_RETRY_DELAY:
        *value = wlan_get_param(vap,IEEE80211_MBO_ASSOC_RETRY_DELAY);
        break;
    case IEEE80211_PARAM_OCE:
        *value = wlan_get_param(vap, IEEE80211_OCE);
        break;
    case IEEE80211_PARAM_OCE_ASSOC_REJECT:
        *value = wlan_get_param(vap, IEEE80211_OCE_ASSOC_REJECT);
        break;
    case IEEE80211_PARAM_OCE_ASSOC_MIN_RSSI:
        *value = wlan_get_param(vap, IEEE80211_OCE_ASSOC_MIN_RSSI);
        break;
    case IEEE80211_PARAM_OCE_ASSOC_RETRY_DELAY:
        *value = wlan_get_param(vap, IEEE80211_OCE_ASSOC_RETRY_DELAY);
        break;
    case IEEE80211_PARAM_OCE_WAN_METRICS:
        *value = wlan_get_param(vap, IEEE80211_OCE_WAN_METRICS);
        break;
    case IEEE80211_PARAM_OCE_HLP:
         *value = wlan_get_param(vap, IEEE80211_OCE_HLP);
         break;
    case IEEE80211_PARAM_NBR_SCAN_PERIOD:
        *value = vap->nbr_scan_period;
        break;
    case IEEE80211_PARAM_RNR:
        *value = vap->rnr_enable;
        break;
    case IEEE80211_PARAM_RNR_FD:
        *value = vap->rnr_enable_fd;
        break;
    case IEEE80211_PARAM_RNR_TBTT:
        *value = vap->rnr_enable_tbtt;
        break;
    case IEEE80211_PARAM_AP_CHAN_RPT:
        *value = vap->ap_chan_rpt_enable;
        break;
    case IEEE80211_PARAM_MGMT_RATE:
        *value = wlan_get_param(vap, IEEE80211_MGMT_RATE);
        break;
    case IEEE80211_PARAM_PRB_RATE:
        *value = wlan_get_param(vap, IEEE80211_PRB_RATE);
        break;
    case IEEE80211_PARAM_RRM_CAP:
        *value = wlan_get_param(vap, IEEE80211_RRM_CAP);
        break;
    case IEEE80211_PARAM_START_ACS_REPORT:
        *value = wlan_get_param(vap, IEEE80211_START_ACS_REPORT);
        break;
    case IEEE80211_PARAM_MIN_DWELL_ACS_REPORT:
        *value = wlan_get_param(vap, IEEE80211_MIN_DWELL_ACS_REPORT);
        break;
    case IEEE80211_PARAM_MAX_SCAN_TIME_ACS_REPORT:
        *value = wlan_get_param(vap, IEEE80211_MAX_SCAN_TIME_ACS_REPORT);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_LONG_DUR:
        *value = wlan_get_param(vap,IEEE80211_ACS_CH_HOP_LONG_DUR);
        break;
    case IEEE80211_PARAM_SCAN_MIN_DWELL:
        *value = wlan_get_param(vap, IEEE80211_SCAN_MIN_DWELL);
        break;
    case IEEE80211_PARAM_SCAN_MAX_DWELL:
        *value = wlan_get_param(vap, IEEE80211_SCAN_MAX_DWELL);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_NO_HOP_DUR:
        *value = wlan_get_param(vap, IEEE80211_ACS_CH_HOP_NO_HOP_DUR);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_CNT_WIN_DUR:
        *value = wlan_get_param(vap,IEEE80211_ACS_CH_HOP_CNT_WIN_DUR);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_NOISE_TH:
        *value = wlan_get_param(vap,IEEE80211_ACS_CH_HOP_NOISE_TH);
        break;
    case IEEE80211_PARAM_ACS_CH_HOP_CNT_TH:
        *value = wlan_get_param(vap,IEEE80211_ACS_CH_HOP_CNT_TH);
        break;
    case IEEE80211_PARAM_ACS_ENABLE_CH_HOP:
        *value = wlan_get_param(vap,IEEE80211_ACS_ENABLE_CH_HOP);
        break;
    case IEEE80211_PARAM_MAX_DWELL_ACS_REPORT:
        *value = wlan_get_param(vap, IEEE80211_MAX_DWELL_ACS_REPORT);
        break;
    case IEEE80211_PARAM_RRM_DEBUG:
        *value = wlan_get_param(vap, IEEE80211_RRM_DEBUG);
	break;
    case IEEE80211_PARAM_RRM_SLWINDOW:
        *value = wlan_get_param(vap, IEEE80211_RRM_SLWINDOW);
	break;
    case IEEE80211_PARAM_RRM_STATS:
        *value = wlan_get_param(vap, IEEE80211_RRM_STATS);
	break;
#if UMAC_SUPPORT_WNM
    case IEEE80211_PARAM_WNM_CAP:
        *value = wlan_get_param(vap, IEEE80211_WNM_CAP);
	break;
    case IEEE80211_PARAM_WNM_BSS_CAP:
        *value = wlan_get_param(vap, IEEE80211_WNM_BSS_CAP);
        break;
    case IEEE80211_PARAM_WNM_TFS_CAP:
        *value = wlan_get_param(vap, IEEE80211_WNM_TFS_CAP);
        break;
    case IEEE80211_PARAM_WNM_TIM_CAP:
        *value = wlan_get_param(vap, IEEE80211_WNM_TIM_CAP);
        break;
    case IEEE80211_PARAM_WNM_SLEEP_CAP:
        *value = wlan_get_param(vap, IEEE80211_WNM_SLEEP_CAP);
        break;
    case IEEE80211_PARAM_WNM_FMS_CAP:
        *value = wlan_get_param(vap, IEEE80211_WNM_FMS_CAP);
	break;
#endif
#ifdef ATHEROS_LINUX_PERIODIC_SCAN
    case IEEE80211_PARAM_PERIODIC_SCAN:
        *value = osifp->os_periodic_scan_period;
        break;
#endif
#if ATH_SW_WOW
    case IEEE80211_PARAM_SW_WOW:
        *value = wlan_get_wow(vap);
        break;
#endif
    case IEEE80211_PARAM_AMPDU:
        ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_AMPDU, value);
        break;
    case IEEE80211_PARAM_AMSDU:
        ucfg_wlan_vdev_mgr_get_param(vdev, WLAN_MLME_CFG_AMSDU, value);
        break;
    case IEEE80211_PARAM_RATE_DROPDOWN:
        if (osifp->osif_is_mode_offload) {
           *value = vap->iv_ratedrop;
        } else {
           qdf_print("This Feature is Supported on Offload Mode Only");
           return -EINVAL;
        }
        break;
    case IEEE80211_PARAM_11N_TX_AMSDU:
        *value = vap->iv_disable_ht_tx_amsdu;
        break;
    case IEEE80211_PARAM_CTSPROT_DTIM_BCN:
        *value = vap->iv_cts2self_prot_dtim_bcn;
        break;
    case IEEE80211_PARAM_VSP_ENABLE:
        *value = vap->iv_enable_vsp;
        break;
    case IEEE80211_PARAM_MAX_AMPDU:
        *value = wlan_get_param(vap, IEEE80211_MAX_AMPDU);
        break;
    case IEEE80211_PARAM_VHT_MAX_AMPDU:
        *value = wlan_get_param(vap, IEEE80211_VHT_MAX_AMPDU);
        break;
#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME
    case IEEE80211_PARAM_REJOINT_ATTEMP_TIME:
        *value = wlan_get_param(vap,IEEE80211_REJOINT_ATTEMP_TIME);
        break;
#endif
    case IEEE80211_PARAM_PWRTARGET:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_PWRTARGET);
        break;
    case IEEE80211_PARAM_COUNTRY_IE:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_IC_COUNTRY_IE);
        break;

    case IEEE80211_PARAM_2G_CSA:
        *value = wlan_get_device_param(ic, IEEE80211_DEVICE_2G_CSA);
        break;

    case IEEE80211_PARAM_CHANBW:
        switch(ic->ic_chanbwflag)
        {
        case IEEE80211_CHAN_HALF:
            *value = 1;
            break;
        case IEEE80211_CHAN_QUARTER:
            *value = 2;
            break;
        default:
            *value = 0;
            break;
        }
        break;
    case IEEE80211_PARAM_MFP_TEST:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_MFP_TEST);
        break;

    case IEEE80211_PARAM_INACT:
        *value = wlan_get_param(vap,IEEE80211_RUN_INACT_TIMEOUT );
        break;
    case IEEE80211_PARAM_INACT_AUTH:
        *value = wlan_get_param(vap,IEEE80211_AUTH_INACT_TIMEOUT );
        break;
    case IEEE80211_PARAM_INACT_INIT:
        *value = wlan_get_param(vap,IEEE80211_INIT_INACT_TIMEOUT );
        break;
    case IEEE80211_PARAM_SESSION_TIMEOUT:
        *value = wlan_get_param(vap,IEEE80211_SESSION_TIMEOUT );
        break;
    case IEEE80211_PARAM_COMPRESSION:
        *value = wlan_get_param(vap, IEEE80211_COMP);
        break;
    case IEEE80211_PARAM_FF:
        *value = wlan_get_param(vap, IEEE80211_FF);
        break;
    case IEEE80211_PARAM_TURBO:
        *value = wlan_get_param(vap, IEEE80211_TURBO);
        break;
    case IEEE80211_PARAM_BURST:
        *value = wlan_get_param(vap, IEEE80211_BURST);
        break;
    case IEEE80211_PARAM_AR:
        *value = wlan_get_param(vap, IEEE80211_AR);
        break;
#if UMAC_SUPPORT_STA_POWERSAVE
    case IEEE80211_PARAM_SLEEP:
        *value = wlan_get_param(vap, IEEE80211_SLEEP);
        break;
#endif
    case IEEE80211_PARAM_EOSPDROP:
        *value = wlan_get_param(vap, IEEE80211_EOSPDROP);
        break;
    case IEEE80211_PARAM_DFSDOMAIN:
        *value = wlan_get_param(vap, IEEE80211_DFSDOMAIN);
        break;
    case IEEE80211_PARAM_WDS_AUTODETECT:
        *value = wlan_get_param(vap, IEEE80211_WDS_AUTODETECT);
        break;
    case IEEE80211_PARAM_WEP_TKIP_HT:
        *value = wlan_get_param(vap, IEEE80211_WEP_TKIP_HT);
        break;
    /*
    ** Support for returning the radio number
    */
    case IEEE80211_PARAM_ATH_RADIO:
		*value = wlan_get_param(vap, IEEE80211_ATH_RADIO);
        break;
    case IEEE80211_PARAM_IGNORE_11DBEACON:
        *value = wlan_get_param(vap, IEEE80211_IGNORE_11DBEACON);
        break;
#if ATH_SUPPORT_WAPI
    case IEEE80211_PARAM_WAPIREKEY_USK:
        *value = wlan_get_wapirekey_unicast(vap);
        break;
    case IEEE80211_PARAM_WAPIREKEY_MSK:
        *value = wlan_get_wapirekey_multicast(vap);
        break;
#endif

#ifdef QCA_PARTNER_PLATFORM
    case IEEE80211_PARAM_PLTFRM_PRIVATE:
        *value = wlan_pltfrm_get_param(vap);
        break;
#endif
    case IEEE80211_PARAM_NO_STOP_DISASSOC:
        *value = osifp->no_stop_disassoc;
        break;
#if UMAC_SUPPORT_VI_DBG

    case IEEE80211_PARAM_DBG_CFG:
        *value = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_DBG_CFG);
        break;

    case IEEE80211_PARAM_RESTART:
        *value = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RESTART);
        break;
    case IEEE80211_PARAM_RXDROP_STATUS:
        *value = ieee80211_vi_dbg_get_param(vap, IEEE80211_VI_RXDROP_STATUS);
        break;
#endif

#if ATH_SUPPORT_IBSS_DFS
    case IEEE80211_PARAM_IBSS_DFS_PARAM:
        *value = vap->iv_ibss_dfs_csa_threshold << 16 |
                   vap->iv_ibss_dfs_csa_measrep_limit << 8 |
                   vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt;
        qdf_print("channel swith time %d measurement report %d recover time %d ",
                 vap->iv_ibss_dfs_csa_threshold,
                 vap->iv_ibss_dfs_csa_measrep_limit ,
                 vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt);
        break;
#endif

#ifdef ATH_SUPPORT_TxBF
    case IEEE80211_PARAM_TXBF_AUTO_CVUPDATE:
        *value = wlan_get_param(vap, IEEE80211_TXBF_AUTO_CVUPDATE);
        break;
    case IEEE80211_PARAM_TXBF_CVUPDATE_PER:
        *value = wlan_get_param(vap, IEEE80211_TXBF_CVUPDATE_PER);
        break;
#endif
    case IEEE80211_PARAM_SCAN_BAND:
        *value = osifp->os_scan_band;
        break;

    case IEEE80211_PARAM_SCAN_CHAN_EVENT:
        if (osifp->osif_is_mode_offload &&
            wlan_vap_get_opmode(vap) == IEEE80211_M_HOSTAP) {
            *value = osifp->is_scan_chevent;
        } else {
            qdf_print("IEEE80211_PARAM_SCAN_CHAN_EVENT is valid only for 11ac "
                   "offload, and in IEEE80211_M_HOSTAP(Access Point) mode");
            retv = EOPNOTSUPP;
            *value = 0;
        }
        break;
    case IEEE80211_PARAM_CONNECTION_SM_STATE:
        *value = 0;
        if (osifp->os_opmode == IEEE80211_M_STA) {
            *value = wlan_connection_sm_get_param(osifp->sm_handle,
                                                    WLAN_CONNECTION_PARAM_CURRENT_STATE);
        }
        break;

#if ATH_SUPPORT_WIFIPOS
    case IEEE80211_PARAM_WIFIPOS_TXCORRECTION:
	*value = ieee80211_wifipos_get_txcorrection(vap);
   	break;

    case IEEE80211_PARAM_WIFIPOS_RXCORRECTION:
	*value = ieee80211_wifipos_get_rxcorrection(vap);
   	break;
#endif

    case IEEE80211_PARAM_ROAMING:
        *value = ic->ic_roaming;
        break;
#if UMAC_SUPPORT_PROXY_ARP
    case IEEE80211_PARAM_PROXYARP_CAP:
        *value = wlan_get_param(vap, IEEE80211_PROXYARP_CAP);
	    break;
#if UMAC_SUPPORT_DGAF_DISABLE
    case IEEE80211_PARAM_DGAF_DISABLE:
        *value = wlan_get_param(vap, IEEE80211_DGAF_DISABLE);
	    break;
#endif
#endif
#if UMAC_SUPPORT_HS20_L2TIF
    case IEEE80211_PARAM_L2TIF_CAP:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_APBRIDGE) ? 0 : 1;
        break;
#endif
    case IEEE80211_PARAM_EXT_IFACEUP_ACS:
        *value = wlan_get_param(vap, IEEE80211_EXT_IFACEUP_ACS);
        break;

    case IEEE80211_PARAM_EXT_ACS_IN_PROGRESS:
        *value = wlan_get_param(vap, IEEE80211_EXT_ACS_IN_PROGRESS);
        break;

    case IEEE80211_PARAM_SEND_ADDITIONAL_IES:
        *value = wlan_get_param(vap, IEEE80211_SEND_ADDITIONAL_IES);
        break;

    case IEEE80211_PARAM_DESIRED_CHANNEL:
        *value = wlan_get_param(vap, IEEE80211_DESIRED_CHANNEL);
        break;

    case IEEE80211_PARAM_DESIRED_PHYMODE:
        *value = wlan_get_param(vap, IEEE80211_DESIRED_PHYMODE);
        break;

    case IEEE80211_PARAM_GET_FREQUENCY:
        *value = ieee80211_ucfg_get_freq(vap);
        break;

    case IEEE80211_PARAM_APONLY:
#if UMAC_SUPPORT_APONLY
        *value = vap->iv_aponly;
#else
        qdf_print("APONLY not enabled");
#endif
        break;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    case IEEE80211_PARAM_NOPBN:
        *value = wlan_get_param(vap, IEEE80211_NOPBN);
	break;
#endif
#if ATH_SUPPORT_DSCP_OVERRIDE
	case IEEE80211_PARAM_DSCP_MAP_ID:
		*value = wlan_get_param(vap, IEEE80211_DSCP_MAP_ID);
	break;
	case IEEE80211_PARAM_DSCP_TID_MAP:
		qdf_print("Get dscp_tid map");
		*value = ieee80211_ucfg_vap_get_dscp_tid_map(vap, value[1]);
	break;
        case IEEE80211_PARAM_VAP_DSCP_PRIORITY:
                *value = wlan_get_param(vap, IEEE80211_VAP_DSCP_PRIORITY);
        break;
#endif
#if ATH_SUPPORT_WRAP
    case IEEE80211_PARAM_PARENT_IFINDEX:
        *value = osifp->os_comdev->ifindex;
        break;

    case IEEE80211_PARAM_PROXY_STA:
        *value = vap->iv_psta;
        break;
#endif
#if RX_CHECKSUM_OFFLOAD
    case IEEE80211_PARAM_RX_CKSUM_ERR_STATS:
	{
	    if(osifp->osif_is_mode_offload) {
                ic->ic_vap_get_param(vap, IEEE80211_RX_CKSUM_ERR_STATS_GET);
	    } else
		qdf_print("RX Checksum Offload Supported only for 11AC VAP ");
	    break;
	}
    case IEEE80211_PARAM_RX_CKSUM_ERR_RESET:
	{
	    if(osifp->osif_is_mode_offload) {
                ic->ic_vap_get_param(vap, IEEE80211_RX_CKSUM_ERR_RESET_GET);
	    } else
		qdf_print("RX Checksum Offload Supported only for 11AC VAP ");
	    break;
	}

#endif /* RX_CHECKSM_OFFLOAD */

#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
    case IEEE80211_PARAM_TSO_STATS:
	{
	    if(osifp->osif_is_mode_offload) {
		ic->ic_vap_get_param(vap, IEEE80211_TSO_STATS_GET);
	    } else
		qdf_print("TSO Supported only for 11AC VAP ");
	    break;
	}
    case IEEE80211_PARAM_TSO_STATS_RESET:
	{
	    if(osifp->osif_is_mode_offload) {
                ic->ic_vap_get_param(vap, IEEE80211_TSO_STATS_RESET_GET);
	    } else
		qdf_print("TSO Supported only for 11AC VAP ");
	    break;
	}
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */

#if HOST_SW_SG_ENABLE
    case IEEE80211_PARAM_SG_STATS:
	{
	    if(osifp->osif_is_mode_offload) {
		ic->ic_vap_get_param(vap, IEEE80211_SG_STATS_GET);
	    } else
		qdf_print("SG Supported only for 11AC VAP ");
	    break;
	}
    case IEEE80211_PARAM_SG_STATS_RESET:
	{
	    if(osifp->osif_is_mode_offload) {
		ic->ic_vap_get_param(vap, IEEE80211_SG_STATS_RESET_GET);
	    } else {
		qdf_print("SG Supported only for 11AC VAP ");
            }
	    break;
	}
#endif /* HOST_SW_SG_ENABLE */

#if HOST_SW_LRO_ENABLE
    case IEEE80211_PARAM_LRO_STATS:
	{
	     if(osifp->osif_is_mode_offload) {
		qdf_print("Aggregated packets:  %d", vap->aggregated);
		qdf_print("Flushed packets:     %d", vap->flushed);
	     } else
		qdf_print("LRO Supported only for 11AC VAP ");
	     break;
	}
    case IEEE80211_PARAM_LRO_STATS_RESET:
	{
	     if(osifp->osif_is_mode_offload) {
		vap->aggregated = 0;
		vap->flushed = 0;
	     } else
		qdf_print("LRO Supported only for 11AC VAP ");
	     break;
	}
#endif /* HOST_SW_LRO_ENABLE */


    case IEEE80211_PARAM_MAX_SCANENTRY:
        *value = wlan_get_param(vap, IEEE80211_MAX_SCANENTRY);
        break;
    case IEEE80211_PARAM_SCANENTRY_TIMEOUT:
        *value = wlan_get_param(vap, IEEE80211_SCANENTRY_TIMEOUT);
        break;
#if ATH_PERF_PWR_OFFLOAD
    case IEEE80211_PARAM_VAP_TX_ENCAP_TYPE:
        *value = wlan_get_param(vap, IEEE80211_VAP_TX_ENCAP_TYPE);
        switch (*value)
        {
            case 0:
                qdf_print("Encap type: Raw");
                break;
            case 1:
                qdf_print("Encap type: Native Wi-Fi");
                break;
            case 2:
                qdf_print("Encap type: Ethernet");
                break;
            default:
                qdf_print("Encap type: Unknown");
                break;
        }
        break;
    case IEEE80211_PARAM_VAP_RX_DECAP_TYPE:
        *value = wlan_get_param(vap, IEEE80211_VAP_RX_DECAP_TYPE);
        switch (*value)
        {
            case 0:
                qdf_print("Decap type: Raw");
                break;
            case 1:
                qdf_print("Decap type: Native Wi-Fi");
                break;
            case 2:
                qdf_print("Decap type: Ethernet");
                break;
            default:
                qdf_print("Decap type: Unknown");
                break;
        }
        break;
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    case IEEE80211_PARAM_RAWMODE_SIM_TXAGGR:
        *value = wlan_get_param(vap, IEEE80211_RAWMODE_SIM_TXAGGR);
        break;
    case IEEE80211_PARAM_RAWMODE_PKT_SIM_STATS:
        *value = wlan_get_param(vap, IEEE80211_RAWMODE_PKT_SIM_STATS);
        break;
    case IEEE80211_PARAM_RAWMODE_SIM_DEBUG:
        *value = wlan_get_param(vap, IEEE80211_RAWMODE_SIM_DEBUG);
        break;
#endif /* QCA_SUPPORT_RAWMODE_PKT_SIMULATION */
#endif /* ATH_PERF_PWR_OFFLOAD */
 case IEEE80211_PARAM_VAP_ENHIND:
        *value  = wlan_get_param(vap, IEEE80211_FEATURE_VAP_ENHIND);
        break;
    case IEEE80211_PARAM_VAP_PAUSE_SCAN:
        *value = vap->iv_pause_scan;
        break;
#if ATH_GEN_RANDOMNESS
    case IEEE80211_PARAM_RANDOMGEN_MODE:
        *value = ic->random_gen_mode;
        break;
#endif
    case IEEE80211_PARAM_WHC_APINFO_WDS:
        *value = son_has_whc_apinfo_flag(
                vap->iv_bss->peer_obj, IEEE80211_NODE_WHC_APINFO_WDS);
        break;
    case IEEE80211_PARAM_WHC_APINFO_SON:
        *value = son_has_whc_apinfo_flag(
                vap->iv_bss->peer_obj, IEEE80211_NODE_WHC_APINFO_SON);
        break;
    case IEEE80211_PARAM_WHC_APINFO_ROOT_DIST:
        *value = ucfg_son_get_root_dist(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_WHC_APINFO_SFACTOR:
        *value =  ucfg_son_get_scaling_factor(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_WHC_SKIP_HYST:
        *value =  ucfg_son_get_skip_hyst(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_WHC_APINFO_BSSID:
    {
        char addr[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
        ieee80211_ssid  *desired_ssid = NULL;
        int retval;
        struct wlan_ssid ssidname;

        OS_MEMSET(&ssidname, 0, sizeof(struct wlan_ssid));
        retval = ieee80211_get_desired_ssid(vap, 0,&desired_ssid);
        if (desired_ssid == NULL)
            return -EINVAL;

        OS_MEMCPY(&ssidname.ssid,&desired_ssid->ssid, desired_ssid->len);
        ssidname.length = desired_ssid->len;
        ucfg_son_find_best_uplink_bssid(vap->vdev_obj, addr,&ssidname);
        macaddr_num_to_str(addr, extra);
    }
    break;
    case IEEE80211_PARAM_WHC_APINFO_RATE:
        *value = (int)son_ucfg_rep_datarate_estimator(
            son_get_backhaul_rate(vap->vdev_obj, true),
            son_get_backhaul_rate(vap->vdev_obj, false),
            (ucfg_son_get_root_dist(vap->vdev_obj) - 1),
            ucfg_son_get_scaling_factor(vap->vdev_obj));
    break;
    case IEEE80211_PARAM_WHC_APINFO_CAP_BSSID:
    {
        u_int8_t addr[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

        son_ucfg_find_cap_bssid(vap->vdev_obj, addr);
        macaddr_num_to_str(addr, extra);
    }
    break;
    case IEEE80211_PARAM_WHC_APINFO_BEST_UPLINK_OTHERBAND_BSSID:
    {
	u_int8_t addr[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

        ucfg_son_get_best_otherband_uplink_bssid(vap->vdev_obj, addr);
        macaddr_num_to_str(addr, extra);
    }
    break;
    case IEEE80211_PARAM_WHC_APINFO_OTHERBAND_UPLINK_BSSID:
    {
        u_int8_t addr[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

	ucfg_son_get_otherband_uplink_bssid(vap->vdev_obj, addr);
	macaddr_num_to_str(addr, extra);
    }
    break;
#if QCA_SUPPORT_SON
    case IEEE80211_PARAM_WHC_APINFO_UPLINK_RATE:
        *value = ucfg_son_get_uplink_rate(vap->vdev_obj);
    break;
    case IEEE80211_PARAM_WHC_APINFO_UPLINK_SNR:
        *value = ucfg_son_get_uplink_snr(vap->vdev_obj);
    break;
#endif
    case IEEE80211_PARAM_WHC_CURRENT_CAP_RSSI:
	    ucfg_son_get_cap_snr(vap->vdev_obj, value);
    break;
    case IEEE80211_PARAM_WHC_CAP_RSSI:
        *value = ucfg_son_get_cap_rssi(vap->vdev_obj);
    break;
    case IEEE80211_PARAM_SON:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_SON);
    break;
    case IEEE80211_PARAM_SON_NUM_VAP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_FEATURE_SON_NUM_VAP);
    break;
    case IEEE80211_PARAM_REPT_MULTI_SPECIAL:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_REPT_MULTI_SPECIAL);
    break;
    case IEEE80211_PARAM_RX_SIGNAL_DBM:
        if (!osifp->osif_is_mode_offload){
            int8_t signal_dbm[6];

            *value = ic->ic_get_rx_signal_dbm(ic, signal_dbm);

            qdf_print("Signal Strength in dBm [ctrl chain 0]: %d", signal_dbm[0]);
            qdf_print("Signal Strength in dBm [ctrl chain 1]: %d", signal_dbm[1]);
            qdf_print("Signal Strength in dBm [ctrl chain 2]: %d", signal_dbm[2]);
            qdf_print("Signal Strength in dBm [ext chain 0]: %d", signal_dbm[3]);
            qdf_print("Signal Strength in dBm [ext chain 1]: %d", signal_dbm[4]);
            qdf_print("Signal Strength in dBm [ext chain 2]: %d", signal_dbm[5]);
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"IEEE80211_PARAM_RX_SIGNAL_DBM is valid only for DA not supported for offload \n");
            retv = EOPNOTSUPP;
            *value = 0;
        }
	break;
    default:
        retv = EOPNOTSUPP;
        break;

   case IEEE80211_PARAM_DFS_CACTIMEOUT:
#if ATH_SUPPORT_DFS
        if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return -1;
        }
        retv = mlme_dfs_get_override_cac_timeout(pdev, &tmp);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
        if (retv == 0)
            *value = tmp;
        else
            retv = EOPNOTSUPP;
        break;
#else
        retv = EOPNOTSUPP;
        break;
#endif /* ATH_SUPPORT_DFS */

   case IEEE80211_PARAM_ENABLE_RTSCTS:
       *value = wlan_get_param(vap, IEEE80211_ENABLE_RTSCTS);
       break;

   case IEEE80211_PARAM_RC_NUM_RETRIES:
       *value = wlan_get_param(vap, IEEE80211_RC_NUM_RETRIES);
       break;
   case IEEE80211_PARAM_256QAM_2G:
       *value = wlan_get_param(vap, IEEE80211_256QAM);
       break;
   case IEEE80211_PARAM_11NG_VHT_INTEROP:
       if (osifp->osif_is_mode_offload) {
            *value = wlan_get_param(vap, IEEE80211_11NG_VHT_INTEROP);
       } else {
            qdf_print("Not supported in this Vap");
       }
       break;
#if UMAC_VOW_DEBUG
    case IEEE80211_PARAM_VOW_DBG_ENABLE:
        *value = (int)osifp->vow_dbg_en;
        break;
#endif
#if WLAN_SUPPORT_SPLITMAC
    case IEEE80211_PARAM_SPLITMAC:
        *value = splitmac_get_enabled_flag(vdev);
        break;
#endif
    case IEEE80211_PARAM_IMPLICITBF:
        *value = wlan_get_param(vap, IEEE80211_SUPPORT_IMPLICITBF);
        break;

    case IEEE80211_PARAM_VHT_SUBFEE:
        *value = wlan_get_param(vap, IEEE80211_VHT_SUBFEE);
        break;

    case IEEE80211_PARAM_VHT_MUBFEE:
        *value = wlan_get_param(vap, IEEE80211_VHT_MUBFEE);
        break;

    case IEEE80211_PARAM_VHT_SUBFER:
        *value = wlan_get_param(vap, IEEE80211_VHT_SUBFER);
        break;

    case IEEE80211_PARAM_VHT_MUBFER:
        *value = wlan_get_param(vap, IEEE80211_VHT_MUBFER);
        break;

    case IEEE80211_PARAM_VHT_STS_CAP:
        *value = wlan_get_param(vap, IEEE80211_VHT_BF_STS_CAP);
        break;

    case IEEE80211_PARAM_VHT_SOUNDING_DIM:
        *value = wlan_get_param(vap, IEEE80211_VHT_BF_SOUNDING_DIM);
        break;

    case IEEE80211_PARAM_VHT_MCS_10_11_SUPP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_VHT_MCS_10_11_SUPP);
        break;

    case IEEE80211_PARAM_VHT_MCS_10_11_NQ2Q_PEER_SUPP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_VHT_MCS_10_11_NQ2Q_PEER_SUPP);
        break;

#if QCA_AIRTIME_FAIRNESS
    case IEEE80211_PARAM_ATF_TXBUF_SHARE:
        *value = ucfg_atf_get_txbuf_share(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_ATF_TXBUF_MAX:
        *value = ucfg_atf_get_max_txbufs(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_ATF_TXBUF_MIN:
        *value = ucfg_atf_get_min_txbufs(vap->vdev_obj);
        break;
    case  IEEE80211_PARAM_ATF_OPT:
        *value = ucfg_atf_get(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_ATF_OVERRIDE_AIRTIME_TPUT:
        *value =  ucfg_atf_get_airtime_tput(vap->vdev_obj);
        break;
    case  IEEE80211_PARAM_ATF_PER_UNIT:
        *value = ucfg_atf_get_per_unit(ic->ic_pdev_obj);
        break;
    case  IEEE80211_PARAM_ATF_MAX_CLIENT:
        {
            int val = ucfg_atf_get_maxclient(ic->ic_pdev_obj);
            if (val < 0)
                retv = EOPNOTSUPP;
            else
                *value = val;
        }
        break;
    case  IEEE80211_PARAM_ATF_SSID_GROUP:
        *value = ucfg_atf_get_ssidgroup(ic->ic_pdev_obj);
        break;
    case IEEE80211_PARAM_ATF_SSID_SCHED_POLICY:
        *value = ucfg_atf_get_ssid_sched_policy(vap->vdev_obj);
        break;
#endif
#if ATH_SSID_STEERING
    case IEEE80211_PARAM_VAP_SSID_CONFIG:
        *value = wlan_get_param(vap, IEEE80211_VAP_SSID_CONFIG);
        break;
#endif

    case IEEE80211_PARAM_TX_MIN_POWER:
        *value = ic->ic_curchan->ic_minpower;
        qdf_print("Get IEEE80211_PARAM_TX_MIN_POWER *value=%d",*value);
        break;
    case IEEE80211_PARAM_TX_MAX_POWER:
        *value = ic->ic_curchan->ic_maxpower;
        qdf_print("Get IEEE80211_PARAM_TX_MAX_POWER *value=%d",*value);
        break;
    case IEEE80211_PARAM_AMPDU_DENSITY_OVERRIDE:
        if(ic->ic_mpdudensityoverride & 0x1) {
            *value = ic->ic_mpdudensityoverride >> 1;
        } else {
            *value = -1;
        }
        break;

    case IEEE80211_PARAM_SMART_MESH_CONFIG:
        *value = wlan_get_param(vap, IEEE80211_SMART_MESH_CONFIG);
        break;

#if MESH_MODE_SUPPORT
    case IEEE80211_PARAM_MESH_CAPABILITIES:
        *value = wlan_get_param(vap, IEEE80211_MESH_CAPABILITIES);
        break;

    case IEEE80211_PARAM_CONFIG_MGMT_TX_FOR_MESH:
         *value = wlan_get_param(vap, IEEE80211_CONFIG_MGMT_TX_FOR_MESH);
         break;

    case IEEE80211_PARAM_MESH_MCAST:
         *value = wlan_get_param(vap, IEEE80211_CONFIG_MESH_MCAST);
#endif

    case IEEE80211_PARAM_RX_FILTER_MONITOR:
        if(IEEE80211_M_MONITOR != vap->iv_opmode && !vap->iv_smart_monitor_vap && !vap->iv_special_vap_mode) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Not monitor VAP or Smart Monitor VAP!\n");
            return -EINVAL;
        }
#if ATH_PERF_PWR_OFFLOAD
        if(osifp->osif_is_mode_offload) {
           *value =  (ic->mon_filter_osif_mac ? MON_FILTER_TYPE_OSIF_MAC : 0)|
                       ic->ic_vap_get_param(vap, IEEE80211_RX_FILTER_MONITOR);
        }
        else {
            *value = 0;
            if (ic->mon_filter_osif_mac) {
                *value |= MON_FILTER_TYPE_OSIF_MAC;
            }
            if (ic->mon_filter_mcast_data) {
                *value |= MON_FILTER_TYPE_MCAST_DATA;
            }
            if (ic->mon_filter_ucast_data) {
                *value |= MON_FILTER_TYPE_UCAST_DATA;
            }
            if (ic->mon_filter_non_data) {
                *value |= MON_FILTER_TYPE_NON_DATA;
            }
        }
#else
        *value = 0;
        if (ic->mon_filter_osif_mac) {
            *value |= MON_FILTER_TYPE_OSIF_MAC;
        }
        if (ic->mon_filter_mcast_data) {
            *value |= MON_FILTER_TYPE_MCAST_DATA;
        }
        if (ic->mon_filter_ucast_data) {
            *value |= MON_FILTER_TYPE_UCAST_DATA;
        }
        if (ic->mon_filter_non_data) {
            *value |= MON_FILTER_TYPE_NON_DATA;
        }
#endif
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_IOCTL,
                  "osif MAC filter=%d\n", ic->mon_filter_osif_mac);
        break;

    case IEEE80211_PARAM_RX_FILTER_SMART_MONITOR:
        if(vap->iv_smart_monitor_vap) {
            *value = 1;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Smart Monitor VAP!\n");
        } else {
            *value = 0;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Not Smart Monitor VAP!\n");
        }
        break;
    case IEEE80211_PARAM_CONFIG_ASSOC_WAR_160W:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_ASSOC_WAR_160W);
        break;
     case IEEE80211_PARAM_CONFIG_MU_CAP_WAR:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_MU_CAP_WAR);
        break;
     case IEEE80211_PARAM_CONFIG_MU_CAP_TIMER:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_MU_CAP_TIMER);
        break;
    case IEEE80211_PARAM_RAWMODE_PKT_SIM:
        *value = wlan_get_param(vap, IEEE80211_RAWMODE_PKT_SIM);
        break;
    case IEEE80211_PARAM_CONFIG_RAW_DWEP_IND:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_RAW_DWEP_IND);
        break;
    case IEEE80211_PARAM_CUSTOM_CHAN_LIST:
        *value = wlan_get_param(vap,IEEE80211_CONFIG_PARAM_CUSTOM_CHAN_LIST);
        break;
#if UMAC_SUPPORT_ACFG
    case IEEE80211_PARAM_DIAG_WARN_THRESHOLD:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_DIAG_WARN_THRESHOLD);
        break;
    case IEEE80211_PARAM_DIAG_ERR_THRESHOLD:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_DIAG_ERR_THRESHOLD);
        break;
#endif
    case IEEE80211_PARAM_CONFIG_REV_SIG_160W:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_REV_SIG_160W);
        break;
    case IEEE80211_PARAM_DISABLE_SELECTIVE_HTMCS_FOR_VAP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_DISABLE_SELECTIVE_HTMCS);
        break;
    case IEEE80211_PARAM_CONFIGURE_SELECTIVE_VHTMCS_FOR_VAP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_CONFIGURE_SELECTIVE_VHTMCS);
        break;
    case IEEE80211_PARAM_RDG_ENABLE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_RDG_ENABLE);
        break;
    case IEEE80211_PARAM_DFS_SUPPORT:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_DFS_SUPPORT);
        break;
    case IEEE80211_PARAM_DFS_ENABLE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_DFS_ENABLE);
        break;
    case IEEE80211_PARAM_ACS_SUPPORT:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_ACS_SUPPORT);
        break;
    case IEEE80211_PARAM_SSID_STATUS:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_SSID_STATUS);
        break;
    case IEEE80211_PARAM_DL_QUEUE_PRIORITY_SUPPORT:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_DL_QUEUE_PRIORITY_SUPPORT);
        break;
    case IEEE80211_PARAM_CLEAR_MIN_MAX_RSSI:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_CLEAR_MIN_MAX_RSSI);
        break;
    case IEEE80211_PARAM_WATERMARK_THRESHOLD:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_WATERMARK_THRESHOLD);
        break;
    case IEEE80211_PARAM_WATERMARK_REACHED:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_WATERMARK_REACHED);
        break;
    case IEEE80211_PARAM_ASSOC_REACHED:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_ASSOC_REACHED);
        break;
    case IEEE80211_PARAM_ENABLE_VENDOR_IE:
	 *value = vap->iv_ena_vendor_ie;
        break;
    case IEEE80211_PARAM_CONFIG_ASSOC_DENIAL_NOTIFY:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_ASSOC_DENIAL_NOTIFICATION);
        break;
   case IEEE80211_PARAM_MACCMD_SEC:
        *value = wlan_get_acl_policy(vap, IEEE80211_ACL_FLAG_ACL_LIST_2);
        break;
    case IEEE80211_PARAM_CONFIG_MON_DECODER:
        if (IEEE80211_M_MONITOR != vap->iv_opmode && !vap->iv_smart_monitor_vap) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Not Monitor VAP!\n");
            return -EINVAL;
        }
        /* monitor vap decoder header type: radiotap=0(default) prism=1 */
        *value = wlan_get_param(vap, IEEE80211_CONFIG_MON_DECODER);
        break;
    case IEEE80211_PARAM_BEACON_RATE_FOR_VAP:
        if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
            *value = wlan_get_param(vap, IEEE80211_BEACON_RATE_FOR_VAP);
        }
        break;
    case IEEE80211_PARAM_SIFS_TRIGGER_RATE:
        if (!ic->ic_is_mode_offload(ic))
            return -EPERM;
        *value = vap->iv_sifs_trigger_rate;
        break;
    case IEEE80211_PARAM_DISABLE_SELECTIVE_LEGACY_RATE_FOR_VAP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_DISABLE_SELECTIVE_LEGACY_RATE);
        break;
    case IEEE80211_PARAM_CONFIG_NSTSCAP_WAR:
        *value = wlan_get_param(vap,IEEE80211_CONFIG_NSTSCAP_WAR);
        break;
    case IEEE80211_PARAM_CHANNEL_SWITCH_MODE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_CHANNEL_SWITCH_MODE);
        break;

    case IEEE80211_PARAM_ENABLE_ECSA_IE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_ECSA_IE);
        break;

    case IEEE80211_PARAM_ECSA_OPCLASS:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_ECSA_OPCLASS);
        break;

    case IEEE80211_PARAM_BACKHAUL:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_BACKHAUL);
        break;

#if DYNAMIC_BEACON_SUPPORT
    case IEEE80211_PARAM_DBEACON_EN:
        *value = vap->iv_dbeacon;
        break;

    case IEEE80211_PARAM_DBEACON_RSSI_THR:
        *value = vap->iv_dbeacon_rssi_thr;
        break;

    case IEEE80211_PARAM_DBEACON_TIMEOUT:
        *value = vap->iv_dbeacon_timeout;
        break;
#endif
#if QCN_IE
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_ENABLE:
        *value = vap->iv_bpr_enable;
        break;
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_LATENCY_COMPENSATION:
        *value = ic->ic_bpr_latency_comp;
        break;
    case IEEE80211_PARAM_BEACON_LATENCY_COMPENSATION:
        *value = ic->ic_bcn_latency_comp;
        break;
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_DELAY:
        *value = vap->iv_bpr_delay;
        break;
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_STATS:
        *value = 0;
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            QDF_TRACE(QDF_MODULE_ID_IOCTL, QDF_TRACE_LEVEL_FATAL,
                      "------------------------------------\n");
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR feature enabled        - %d  |\n", vap->iv_bpr_enable);

            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR Latency compensation   - %d ms |\n", ic->ic_bpr_latency_comp);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| Beacon Latency compensation- %d ms |\n", ic->ic_bcn_latency_comp);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR delay                  - %d ms |\n", vap->iv_bpr_delay);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| Current Timestamp          - %lld |\n", qdf_ktime_to_ns(ktime_get()));
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| Next beacon Timestamp      - %lld |\n", qdf_ktime_to_ns(vap->iv_next_beacon_tstamp));
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| Timer expires in           - %lld |\n", qdf_ktime_to_ns(qdf_ktime_add(qdf_ktime_get(),
                      qdf_hrtimer_get_remaining(&vap->bpr_timer))));
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR timer start count      - %u |\n", vap->iv_bpr_timer_start_count);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR unicast probresp count - %u |\n", vap->iv_bpr_unicast_resp_count);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR timer resize count     - %u |\n", vap->iv_bpr_timer_resize_count);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR timer callback count   - %u |\n", vap->iv_bpr_callback_count);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "| BPR timer cancel count     - %u |\n", vap->iv_bpr_timer_cancel_count);
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "------------------------------------\n");
        } else {
            QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
                      "Invalid. Allowed only in HOSTAP mode\n");
        }
        break;
    case IEEE80211_PARAM_BCAST_PROBE_RESPONSE_STATS_CLEAR:
        *value = 0;
        vap->iv_bpr_timer_start_count  = 0;
        vap->iv_bpr_timer_resize_count = 0;
        vap->iv_bpr_callback_count     = 0;
        vap->iv_bpr_unicast_resp_count = 0;
        vap->iv_bpr_timer_cancel_count = 0;
        break;
#endif

    case IEEE80211_PARAM_TXPOW_MGMT:
        frame_subtype = txpow_frm_subtype[1];
        if (((frame_subtype & ~IEEE80211_FC0_SUBTYPE_MASK)!=0) || (frame_subtype < IEEE80211_FC0_SUBTYPE_ASSOC_REQ)
                || (frame_subtype > IEEE80211_FC0_SUBTYPE_DEAUTH) ) {
            qdf_print("Invalid value entered for frame subtype");
            return -EINVAL;
        }
        *value = vap->iv_txpow_mgt_frm[(frame_subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)];
        break;
    case IEEE80211_PARAM_TXPOW:
        frame_type = txpow_frm_subtype[1] >> IEEE80211_SUBTYPE_TXPOW_SHIFT;
        frame_subtype = (txpow_frm_subtype[1] & 0xff);
        if (((frame_subtype & ~IEEE80211_FC0_SUBTYPE_MASK)!=0) || (frame_subtype < IEEE80211_FC0_SUBTYPE_ASSOC_REQ)
                || (frame_subtype > IEEE80211_FC0_SUBTYPE_CF_END_ACK) || (frame_type > IEEE80211_FC0_TYPE_DATA) ) {
            return -EINVAL;
        }
        *value = vap->iv_txpow_frm[frame_type >>IEEE80211_FC0_TYPE_SHIFT][(frame_subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)];
        break;
    case IEEE80211_PARAM_CONFIG_TX_CAPTURE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_TX_CAPTURE);
        break;

    case IEEE80211_PARAM_ENABLE_FILS:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_FILS);
    break;
    case IEEE80211_PARAM_HE_EXTENDED_RANGE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_EXTENDED_RANGE);
    break;
    case IEEE80211_PARAM_HE_DCM:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_DCM);
    break;
    case IEEE80211_PARAM_HE_FRAGMENTATION:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_FRAGMENTATION);
    break;
    case IEEE80211_PARAM_HE_MU_EDCA:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_MU_EDCA);
    break;
    case IEEE80211_PARAM_HE_DL_MU_OFDMA:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_DL_MU_OFDMA);
    break;
    case IEEE80211_PARAM_HE_UL_MU_MIMO:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_MU_MIMO);
    break;
    case IEEE80211_PARAM_HE_UL_MU_OFDMA:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_UL_MU_OFDMA);
    break;
    case IEEE80211_PARAM_HE_SU_BFEE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_SU_BFEE);
    break;
    case IEEE80211_PARAM_HE_SU_BFER:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_SU_BFER);
    break;
    case IEEE80211_PARAM_HE_MU_BFEE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_MU_BFEE);
    break;
    case IEEE80211_PARAM_HE_MU_BFER:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_MU_BFER);
    break;
    case IEEE80211_PARAM_EXT_NSS_SUPPORT:
        if (!ic->ic_is_mode_offload(ic)) {
            return -EPERM;
        }
        *value = wlan_get_param(vap, IEEE80211_CONFIG_EXT_NSS_SUPPORT);
    break;
    case IEEE80211_PARAM_QOS_ACTION_FRAME_CONFIG:
        *value = ic->ic_qos_acfrm_config;
    break;
    case IEEE80211_PARAM_HE_LTF:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_LTF);
    break;
    case IEEE80211_PARAM_HE_AR_GI_LTF:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_AR_GI_LTF);
    break;
    case IEEE80211_PARAM_HE_RTSTHRSHLD:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_RTSTHRSHLD);
    break;
    case IEEE80211_PARAM_DFS_INFO_NOTIFY_APP:
        *value = ic->ic_dfs_info_notify_channel_available;
        break;
    case IEEE80211_PARAM_DISABLE_CABQ:
        *value = wlan_get_param(vap, IEEE80211_FEATURE_DISABLE_CABQ);
        break;
#if ATH_ACL_SOFTBLOCKING
    case IEEE80211_PARAM_SOFTBLOCK_WAIT_TIME:
        *value = vap->iv_softblock_wait_time;
        break;
    case IEEE80211_PARAM_SOFTBLOCK_ALLOW_TIME:
        *value = vap->iv_softblock_allow_time;
        break;
#endif
    case IEEE80211_PARAM_CSL_SUPPORT:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_CSL_SUPPORT);
    break;
    case IEEE80211_PARAM_TIMEOUTIE:
        *value = wlan_get_param(vap, IEEE80211_SUPPORT_TIMEOUTIE);
        break;
    case IEEE80211_PARAM_PMF_ASSOC:
        *value = wlan_get_param(vap, IEEE80211_SUPPORT_PMF_ASSOC);
        break;

    case IEEE80211_PARAM_BEST_UL_HYST:
        *value = ucfg_son_get_bestul_hyst(vap->vdev_obj);
        break;
    case IEEE80211_PARAM_HE_TX_MCSMAP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_TX_MCSMAP);
        qdf_print("Getting HE TX MCS MAP set: %x",*value);
        break;
    case IEEE80211_PARAM_HE_RX_MCSMAP:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_RX_MCSMAP);
        qdf_print("Getting HE RX MCS MAP set: %x",*value);
        break;
    case IEEE80211_PARAM_CONFIG_M_COPY:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_M_COPY);
        break;
    case IEEE80211_PARAM_CONFIG_CAPTURE_LATENCY_ENABLE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_CAPTURE_LATENCY_ENABLE);
        break;
    case IEEE80211_PARAM_BA_BUFFER_SIZE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_BA_BUFFER_SIZE);
        break;
    case IEEE80211_PARAM_NSSOL_VAP_READ_RXPREHDR:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_READ_RXPREHDR);
        break;
    case IEEE80211_PARAM_HE_SOUNDING_MODE:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_SOUNDING_MODE);
        break;
    case IEEE80211_PARAM_RSN_OVERRIDE:
        *value = wlan_get_param(vap, IEEE80211_SUPPORT_RSN_OVERRIDE);
        break;
    case IEEE80211_PARAM_MAP:
        *value = son_vdev_map_capability_get(vap->vdev_obj, SON_MAP_CAPABILITY);
        break;
    case IEEE80211_PARAM_MAP_BSS_TYPE:
        *value = son_vdev_map_capability_get(vap->vdev_obj, SON_MAP_CAPABILITY_VAP_TYPE);
        break;
    case IEEE80211_PARAM_HE_HT_CTRL:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_HT_CTRL);
        break;
    case IEEE80211_PARAM_RAWMODE_OPEN_WAR:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_RAWMODE_OPEN_WAR);
        break;
    case IEEE80211_PARAM_MODE:
        *value = wlan_get_param(vap, IEEE80211_DRIVER_HW_CAPS);
        break;
    case IEEE80211_PARAM_FT_ENABLE:
        if(vap->iv_opmode == IEEE80211_M_STA) {
            *value = wlan_get_param(vap, IEEE80211_CONFIG_FT_ENABLE);
        } else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "Valid only in STA mode\n");
            return -EINVAL;
        }
        break;
#if QCA_SUPPORT_SON
    case IEEE80211_PARAM_SON_EVENT_BCAST:
        *value = wlan_son_is_vdev_event_bcast_enabled(vap->vdev_obj);
	break;
    case IEEE80211_PARAM_WHC_MIXEDBH_ULRATE:
        *value = son_get_ul_mixedbh(vap->vdev_obj);
        break;
#endif
#if UMAC_SUPPORT_WPA3_STA
    case IEEE80211_PARAM_SAE_AUTH_ATTEMPTS:
        *value = vap->iv_sae_max_auth_attempts;
        break;
#endif
    case IEEE80211_PARAM_DPP_VAP_MODE:
        *value = vap->iv_dpp_vap_mode;
        break;
    case IEEE80211_PARAM_HE_BSR_SUPPORT:
        *value = wlan_get_param(vap, IEEE80211_CONFIG_HE_BSR_SUPPORT);
    break;
    case IEEE80211_PARAM_MAP_VAP_BEACONING:
         *value = son_vdev_map_capability_get(vap->vdev_obj,
                                     SON_MAP_CAPABILITY_VAP_UP);
         break;
    case IEEE80211_PARAM_UNIFORM_RSSI:
        *value = ic->ic_uniform_rssi;
         break;
    case IEEE80211_PARAM_CSA_INTEROP_PHY:
        *value = vap->iv_csa_interop_phy;
        break;
    case IEEE80211_PARAM_CSA_INTEROP_BSS:
        *value = vap->iv_csa_interop_bss;
        break;
    case IEEE80211_PARAM_CSA_INTEROP_AUTH:
        *value = vap->iv_csa_interop_auth;
        break;
    case IEEE80211_PARAM_GET_RU26_TOLERANCE:
        *value = ic->ru26_tolerant;
        break;
#if SM_ENG_HIST_ENABLE
    case IEEE80211_PARAM_SM_HISTORY:
         wlan_mlme_print_all_sm_history();
         break;
#endif
    }
    if (retv) {
        qdf_print("%s : parameter 0x%x not supported ", __func__, param);
        return -EOPNOTSUPP;
    }

    return retv;
}

u_int32_t ieee80211_ucfg_get_maxphyrate(wlan_if_t vaphandle)
{
 struct ieee80211vap *vap = vaphandle;
 struct ieee80211com *ic = vap->iv_ic;

 if (!vap->iv_bss)
     return 0;

 /* Rate should show 0 if VAP is not UP.
  * Rates are returned as Kbps to avoid
  * signed overflow when using HE modes
  * or larger values of NSS.
  * All applications should handle this.
  */
 return((wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ? 0: ic->ic_get_maxphyrate(ic, vap->iv_bss));
}

#define IEEE80211_MODE_TURBO_STATIC_A   IEEE80211_MODE_MAX

int ieee80211_convert_mode(const char *mode)
{
#define TOUPPER(c) ((((c) > 0x60) && ((c) < 0x7b)) ? ((c) - 0x20) : (c))
    static const struct
    {
        char *name;
        int mode;
    } mappings[] = {
        /* NB: need to order longest strings first for overlaps */
        { "11AST" , IEEE80211_MODE_TURBO_STATIC_A },
        { "AUTO"  , IEEE80211_MODE_AUTO },
        { "11A"   , IEEE80211_MODE_11A },
        { "11B"   , IEEE80211_MODE_11B },
        { "11G"   , IEEE80211_MODE_11G },
        { "FH"    , IEEE80211_MODE_FH },
		{ "0"     , IEEE80211_MODE_AUTO },
		{ "1"     , IEEE80211_MODE_11A },
		{ "2"     , IEEE80211_MODE_11B },
		{ "3"     , IEEE80211_MODE_11G },
		{ "4"     , IEEE80211_MODE_FH },
		{ "5"     , IEEE80211_MODE_TURBO_STATIC_A },
	    { "TA"      , IEEE80211_MODE_TURBO_A },
	    { "TG"      , IEEE80211_MODE_TURBO_G },
	    { "11NAHT20"      , IEEE80211_MODE_11NA_HT20 },
	    { "11NGHT20"      , IEEE80211_MODE_11NG_HT20 },
	    { "11NAHT40PLUS"  , IEEE80211_MODE_11NA_HT40PLUS },
	    { "11NAHT40MINUS" , IEEE80211_MODE_11NA_HT40MINUS },
	    { "11NGHT40PLUS"  , IEEE80211_MODE_11NG_HT40PLUS },
	    { "11NGHT40MINUS" , IEEE80211_MODE_11NG_HT40MINUS },
        { "11NGHT40" , IEEE80211_MODE_11NG_HT40},
        { "11NAHT40" , IEEE80211_MODE_11NA_HT40},
        { "11ACVHT20", IEEE80211_MODE_11AC_VHT20},
        { "11ACVHT40PLUS", IEEE80211_MODE_11AC_VHT40PLUS},
        { "11ACVHT40MINUS", IEEE80211_MODE_11AC_VHT40MINUS},
        { "11ACVHT40", IEEE80211_MODE_11AC_VHT40},
        { "11ACVHT80", IEEE80211_MODE_11AC_VHT80},
        { "11ACVHT160", IEEE80211_MODE_11AC_VHT160},
        { "11ACVHT80_80", IEEE80211_MODE_11AC_VHT80_80},
        { "11AHE20" , IEEE80211_MODE_11AXA_HE20},
        { "11GHE20" , IEEE80211_MODE_11AXG_HE20},
        { "11AHE40PLUS" , IEEE80211_MODE_11AXA_HE40PLUS},
        { "11AHE40MINUS" , IEEE80211_MODE_11AXA_HE40MINUS},
        { "11GHE40PLUS" , IEEE80211_MODE_11AXG_HE40PLUS},
        { "11GHE40MINUS" , IEEE80211_MODE_11AXG_HE40MINUS},
        { "11AHE40" , IEEE80211_MODE_11AXA_HE40},
        { "11GHE40" , IEEE80211_MODE_11AXG_HE40},
        { "11AHE80" , IEEE80211_MODE_11AXA_HE80},
        { "11AHE160" , IEEE80211_MODE_11AXA_HE160},
        { "11AHE80_80" , IEEE80211_MODE_11AXA_HE80_80},
        { NULL }
    };
    int i, j;
    const char *cp;

    for (i = 0; mappings[i].name != NULL; i++) {
        cp = mappings[i].name;
        for (j = 0; j < strlen(mode) + 1; j++) {
            /* convert user-specified string to upper case */
            if (TOUPPER(mode[j]) != cp[j])
                break;
            if (cp[j] == '\0')
                return mappings[i].mode;
        }
    }
    return -1;
#undef TOUPPER
}

int ieee80211_ucfg_set_phymode(wlan_if_t vap, char *modestr, int len)
{
    char s[30];      /* big enough for ``11nght40plus'' */
    int mode;
    uint32_t ldpc;

    /* truncate the len if it shoots our buffer.
     * len does not include '\0'
     */
    if (len >= sizeof(s)) {
        len = sizeof(s) - 1;
    }

    /* Copy upto len characters and a '\0' enforced by strlcpy */
    if (strlcpy(s, modestr, len + 1) > sizeof(s)) {
        qdf_print("String too long to copy");
        return -EINVAL;
    }

    /*
    ** Convert mode name into a specific mode
    */

    mode = ieee80211_convert_mode(s);
    if (mode < 0)
        return -EINVAL;

    /* if ldpc is disabled then as per 802.11ax
     * specification, D2.0 (section 28.1.1) we
     * can not allow mode greater than 20 MHz
     */
    ucfg_wlan_vdev_mgr_get_param(vap->vdev_obj, WLAN_MLME_CFG_LDPC, &ldpc);
    if ((ldpc == IEEE80211_HTCAP_C_LDPC_NONE) &&
            (mode >= IEEE80211_MODE_11AXA_HE40PLUS)) {
        qdf_print("Mode %s is not allowed if LDPC is "
                "already disabled", s);
        return -EINVAL;
    }

    return ieee80211_ucfg_set_wirelessmode(vap, mode);
}

int ieee80211_ucfg_set_wirelessmode(wlan_if_t vap, int mode)
{
    struct ieee80211com *ic = vap->iv_ic;

    /* OBSS scanning should only be enabled in 40 Mhz 2.4G */

    /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on coex rules */
    if (!ic->ic_user_coext_disable) {
            struct ieee80211vap *tmpvap = NULL;
            bool width40_vap_found = false;
            /*Check if already any VAP is configured with HT40 mode*/
            TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                if((vap != tmpvap) &&
                  ((tmpvap->iv_des_mode == IEEE80211_MODE_11NG_HT40PLUS) ||
                   (tmpvap->iv_des_mode == IEEE80211_MODE_11NG_HT40MINUS) ||
                   (tmpvap->iv_des_mode == IEEE80211_MODE_11NG_HT40) ||
                   (tmpvap->iv_des_mode == IEEE80211_MODE_11AXG_HE40PLUS) ||
                   (tmpvap->iv_des_mode == IEEE80211_MODE_11AXG_HE40MINUS) ||
                   (tmpvap->iv_des_mode == IEEE80211_MODE_11AXG_HE40))) {
                    width40_vap_found = true;
                    break;
                }
            }
            /*
            * If any VAP is already configured with 40 width,
            * no need to clear disable coext flag,
            * as disable coext flag may be set by other VAP
            */
            if(!width40_vap_found)
                ieee80211com_clear_flags(ic, IEEE80211_F_COEXT_DISABLE);
    }

#if ATH_SUPPORT_IBSS_HT
    /*
     * config ic adhoc ht capability
     */
    if (vap->iv_opmode == IEEE80211_M_IBSS) {

        wlan_dev_t ic = wlan_vap_get_devhandle(vap);

        switch (mode) {
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NG_HT20:
            /* enable adhoc ht20 and aggr */
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT20ADHOC, 1);
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT40ADHOC, 0);
            break;
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40:
        case IEEE80211_MODE_11NA_HT40:
            /* enable adhoc ht40 and aggr */
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT20ADHOC, 1);
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT40ADHOC, 1);
            break;
        /* TODO: With IBSS support add VHT fields as well */
        default:
            /* clear adhoc ht20, ht40, aggr */
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT20ADHOC, 0);
            wlan_set_device_param(ic, IEEE80211_DEVICE_HT40ADHOC, 0);
            break;
        } /* end of switch (mode) */
    }
#endif /* end of #if ATH_SUPPORT_IBSS_HT */

    return wlan_set_desired_phymode(vap, mode);
}

/*
* Get a key index from a request.  If nothing is
* specified in the request we use the current xmit
* key index.  Otherwise we just convert the index
* to be base zero.
*/
static int getkeyix(wlan_if_t vap, u_int16_t flags, u_int16_t *kix)
{
    int kid;

    kid = flags & IW_ENCODE_INDEX;
    if (kid < 1 || kid > IEEE80211_WEP_NKID)
    {
        kid = wlan_get_default_keyid(vap);
        if (kid == IEEE80211_KEYIX_NONE)
            kid = 0;
    }
    else
        --kid;
    if (0 <= kid && kid < IEEE80211_WEP_NKID)
    {
        *kix = kid;
        return 0;
    }
    else
        return -EINVAL;
}

/*
 * If authmode = IEEE80211_AUTH_OPEN, script apup would skip authmode setup.
 * Do default authmode setup here for OPEN mode.
 */
static int sencode_wep(struct net_device *dev)
{
    osif_dev            *osifp = ath_netdev_priv(dev);
    wlan_if_t           vap    = osifp->os_if;
    int                 error  = 0;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    u_int               nmodes = 1;
    ieee80211_auth_mode modes[1];

    osifp->authmode = IEEE80211_AUTH_OPEN;

    modes[0] = IEEE80211_AUTH_OPEN;
    error = wlan_set_authmodes(vap, modes, nmodes);
#else
    error = wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE, 1 << WLAN_CRYPTO_AUTH_OPEN);
#endif
    if (error == 0 ) {
        error = wlan_set_param(vap, IEEE80211_FEATURE_PRIVACY, 0);
        osifp->uciphers[0] = osifp->mciphers[0] = IEEE80211_CIPHER_NONE;
        osifp->u_count = osifp->m_count = 1;
    }

    return IS_UP(dev) ? -osif_vap_init(dev, RESCAN) : 0;
}

int ieee80211_ucfg_set_encode(wlan_if_t vap, u_int16_t length, u_int16_t flags, void *keybuf)
{
    osif_dev *osifp = (osif_dev *)vap->iv_ifp;
    struct net_device *dev = osifp->netdev;
    ieee80211_keyval key_val;
    u_int16_t kid;
    int error = -EOPNOTSUPP;
    u_int8_t keydata[IEEE80211_KEYBUF_SIZE];
    int wepchange = 0;

    if (ieee80211_crypto_wep_mbssid_enabled())
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 1);  /* wep keys will start from 4 in keycache for support wep multi-bssid */
    else
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 0);  /* wep keys will allocate index 0-3 in keycache */

    if ((flags & IW_ENCODE_DISABLED) == 0)
    {
        /*
         * Enable crypto, set key contents, and
         * set the default transmit key.
         */
        error = getkeyix(vap, flags, &kid);
        if (error)
            return error;
        if (length > IEEE80211_KEYBUF_SIZE)
            return -EINVAL;

        /* XXX no way to install 0-length key */
        if (length > 0)
        {

            /* WEP key length should be 40,104, 128 bits only */
            if(!((length == IEEE80211_KEY_WEP40_LEN) ||
                        (length == IEEE80211_KEY_WEP104_LEN) ||
                        (length == IEEE80211_KEY_WEP128_LEN)))
            {

                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "WEP key is rejected due to key of length %d\n", length);
                osif_ioctl_delete_vap(dev);
                return -EINVAL;
            }

            /*
             * ieee80211_match_rsn_info() IBSS mode need.
             * Otherwise, it caused crash when tx frame find tx rate
             *   by node RateControl info not update.
             */
            if (osifp->os_opmode == IEEE80211_M_IBSS) {
                /* set authmode to IEEE80211_AUTH_OPEN */
                sencode_wep(dev);

                /* set keymgmtset to WPA_ASE_NONE */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
         wlan_crypto_set_vdev_param(vap->vdev_obj,
				  WLAN_CRYPTO_PARAM_KEY_MGMT,
					WPA_ASE_NONE);
#else
                wlan_set_rsn_cipher_param(vap, IEEE80211_KEYMGT_ALGS, WPA_ASE_NONE);
#endif
            }

            OS_MEMCPY(keydata, keybuf, length);
            memset(&key_val, 0, sizeof(ieee80211_keyval));
            key_val.keytype = IEEE80211_CIPHER_WEP;
            key_val.keydir = IEEE80211_KEY_DIR_BOTH;
            key_val.keylen = length;
            key_val.keydata = keydata;
            key_val.macaddr = (u_int8_t *)ieee80211broadcastaddr;

            if (wlan_set_key(vap,kid,&key_val) != 0)
                return -EINVAL;
        }
        else
        {
            /*
             * When the length is zero the request only changes
             * the default transmit key.  Verify the new key has
             * a non-zero length.
             */
            if ( wlan_set_default_keyid(vap,kid) != 0  ) {
                qdf_print("\n Invalid Key is being Set. Bringing VAP down! ");
                osif_ioctl_delete_vap(dev);
                return -EINVAL;
            }
        }
        if (error == 0)
        {
            /*
             * The default transmit key is only changed when:
             * 1. Privacy is enabled and no key matter is
             *    specified.
             * 2. Privacy is currently disabled.
             * This is deduced from the iwconfig man page.
             */
            if (length == 0 ||
                    (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY)) == 0)
                wlan_set_default_keyid(vap,kid);
            wepchange = (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY)) == 0;
            wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 1);
        }
    }
    else
    {
        if (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY) == 0)
            return 0;
        wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY, 0);
        wepchange = 1;
        error = 0;
    }
    if (error == 0)
    {
        /* Set policy for unencrypted frames */
        if ((flags & IW_ENCODE_OPEN) &&
                (!(flags & IW_ENCODE_RESTRICTED)))
        {
            wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 0);
        }
        else if (!(flags & IW_ENCODE_OPEN) &&
                (flags & IW_ENCODE_RESTRICTED))
        {
            wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 1);
        }
        else
        {
            /* Default policy */
            if (wlan_get_param(vap,IEEE80211_FEATURE_PRIVACY))
                wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 1);
            else
                wlan_set_param(vap,IEEE80211_FEATURE_DROP_UNENC, 0);
        }
    }
    if (error == 0 && IS_UP(dev) && wepchange)
    {
        /*
         * Device is up and running; we must kick it to
         * effect the change.  If we're enabling/disabling
         * crypto use then we must re-initialize the device
         * so the 802.11 state machine is reset.  Otherwise
         * the key state should have been updated above.
         */

        error = osif_vap_init(dev, RESCAN);
    }
#ifdef ATH_SUPERG_XR
    /* set the same params on the xr vap device if exists */
    if(!error && vap->iv_xrvap && !(vap->iv_flags & IEEE80211_F_XR))
        ieee80211_ucfg_set_encode(vap, ptr->len, ptr->flags,
                ptr->buff);
#endif
    return error;
}

int ieee80211_ucfg_set_rate(wlan_if_t vap, int value)
{
    int retv;

    retv = wlan_set_param(vap, IEEE80211_FIXED_RATE, value);
    if (EOK == retv) {
        if (value != IEEE80211_FIXED_RATE_NONE) {
            /* set default retries when setting fixed rate */
            retv = wlan_set_param(vap, IEEE80211_FIXED_RETRIES, 4);
        }
        else {
            retv = wlan_set_param(vap, IEEE80211_FIXED_RETRIES, 0);
        }
    }
    return retv;
}

#define IEEE80211_MODE_TURBO_STATIC_A   IEEE80211_MODE_MAX
int ieee80211_ucfg_get_phymode(wlan_if_t vap, char *modestr, u_int16_t *length, int type)
{
    static const struct
    {
        char *name;
        int mode;
    } mappings[] = {
        /* NB: need to order longest strings first for overlaps */
        { "11AST" , IEEE80211_MODE_TURBO_STATIC_A },
        { "AUTO"  , IEEE80211_MODE_AUTO },
        { "11A"   , IEEE80211_MODE_11A },
        { "11B"   , IEEE80211_MODE_11B },
        { "11G"   , IEEE80211_MODE_11G },
        { "FH"    , IEEE80211_MODE_FH },
        { "TA"      , IEEE80211_MODE_TURBO_A },
        { "TG"      , IEEE80211_MODE_TURBO_G },
        { "11NAHT20"        , IEEE80211_MODE_11NA_HT20 },
        { "11NGHT20"        , IEEE80211_MODE_11NG_HT20 },
        { "11NAHT40PLUS"    , IEEE80211_MODE_11NA_HT40PLUS },
        { "11NAHT40MINUS"   , IEEE80211_MODE_11NA_HT40MINUS },
        { "11NGHT40PLUS"    , IEEE80211_MODE_11NG_HT40PLUS },
        { "11NGHT40MINUS"   , IEEE80211_MODE_11NG_HT40MINUS },
        { "11NGHT40"        , IEEE80211_MODE_11NG_HT40},
        { "11NAHT40"        , IEEE80211_MODE_11NA_HT40},
        { "11ACVHT20"       , IEEE80211_MODE_11AC_VHT20},
        { "11ACVHT40PLUS"   , IEEE80211_MODE_11AC_VHT40PLUS},
        { "11ACVHT40MINUS"  , IEEE80211_MODE_11AC_VHT40MINUS},
        { "11ACVHT40"       , IEEE80211_MODE_11AC_VHT40},
        { "11ACVHT80"       , IEEE80211_MODE_11AC_VHT80},
        { "11ACVHT160"      , IEEE80211_MODE_11AC_VHT160},
        { "11ACVHT80_80"    , IEEE80211_MODE_11AC_VHT80_80},
        { "11AHE20"         , IEEE80211_MODE_11AXA_HE20},
        { "11GHE20"         , IEEE80211_MODE_11AXG_HE20},
        { "11AHE40PLUS"     , IEEE80211_MODE_11AXA_HE40PLUS},
        { "11AHE40MINUS"    , IEEE80211_MODE_11AXA_HE40MINUS},
        { "11GHE40PLUS"     , IEEE80211_MODE_11AXG_HE40PLUS},
        { "11GHE40MINUS"    , IEEE80211_MODE_11AXG_HE40MINUS},
        { "11AHE40"         , IEEE80211_MODE_11AXA_HE40},
        { "11GHE40"         , IEEE80211_MODE_11AXG_HE40},
        { "11AHE80"         , IEEE80211_MODE_11AXA_HE80},
        { "11AHE160"        , IEEE80211_MODE_11AXA_HE160},
        { "11AHE80_80"      , IEEE80211_MODE_11AXA_HE80_80},
        { NULL }
    };
    enum ieee80211_phymode  phymode;
    int i;

    if(type == CURR_MODE){
        phymode = wlan_get_current_phymode(vap);
    }else if(type == PHY_MODE){
        phymode = wlan_get_desired_phymode(vap);
    }else{
        IEEE80211_DPRINTF(vap,  IEEE80211_MSG_ANY, "Function %s should be called with a valid type \n ",__func__);
        return -EINVAL;
    }

    for (i = 0; mappings[i].name != NULL ; i++)
    {
        if (phymode == mappings[i].mode)
        {
            *length = strlen(mappings[i].name);
            strlcpy(modestr, mappings[i].name, *length + 1);
            break;
        }
    }
    return 0;
}
#undef IEEE80211_MODE_TURBO_STATIC_A

static size_t
sta_space(const wlan_node_t node, size_t *ielen, wlan_if_t vap)
{
    u_int8_t    ni_ie[IEEE80211_MAX_OPT_IE];
    u_int16_t ni_ie_len = IEEE80211_MAX_OPT_IE;
    u_int8_t *macaddr = wlan_node_getmacaddr(node);
    *ielen = 0;

    if(!wlan_node_getwpaie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getwmeie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getathie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getwpsie(vap, macaddr, ni_ie, &ni_ie_len)) {
        *ielen += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }

    return roundup(sizeof(struct ieee80211req_sta_info) + *ielen,
        sizeof(u_int32_t));
}

void
get_sta_space(void *arg, wlan_node_t node)
{
    struct stainforeq *req = arg;
    size_t ielen;

    /* already ignore invalid nodes in UMAC */
    req->space += sta_space(node, &ielen, req->vap);
}

static uint8_t get_phymode_from_chwidth(struct ieee80211com *ic,
                                        struct ieee80211_node *ni)
{
    enum ieee80211_phymode cur_mode = ni->ni_vap->iv_cur_mode;
    uint8_t curr_phymode = ni->ni_phymode;
    uint8_t mode;

    /* Mode will attain value of 1 for HE */
    mode = !!(ni->ni_ext_flags & IEEE80211_NODE_HE);
    /* Mode will attain value of 2 for VHT and 3 for HT */
    mode = mode ? mode : (!!(ni->ni_flags & IEEE80211_NODE_VHT) ? 2 : (!!(ni->ni_flags &
           IEEE80211_NODE_HT) ? 3 : mode));
    if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) {
        switch(mode) {
            /* HE mode */
            case 1:
            {
                switch (ni->ni_chwidth) {
                    case IEEE80211_CWM_WIDTH20:
                        curr_phymode = IEEE80211_MODE_11AXA_HE20;
                    break;
                    case IEEE80211_CWM_WIDTH40:
                        curr_phymode = IEEE80211_MODE_11AXA_HE40;
                    break;
                    case IEEE80211_CWM_WIDTH80:
                        curr_phymode = IEEE80211_MODE_11AXA_HE80;
                    break;
                    case IEEE80211_CWM_WIDTH160:
                        return curr_phymode;
                    break;
                    default:
                        return curr_phymode;
                    break;
                }
            }
            break;
            /* VHT mode */
            case 2:
            {
                switch (ni->ni_chwidth) {
                    case IEEE80211_CWM_WIDTH20:
                        curr_phymode = IEEE80211_MODE_11AC_VHT20;
                    break;
                    case IEEE80211_CWM_WIDTH40:
                        curr_phymode = IEEE80211_MODE_11AC_VHT40;
                    break;
                    case IEEE80211_CWM_WIDTH80:
                        curr_phymode = IEEE80211_MODE_11AC_VHT80;
                    break;
                    case IEEE80211_CWM_WIDTH160:
                        if ((cur_mode == IEEE80211_MODE_11AXA_HE160) ||
                            (cur_mode == IEEE80211_MODE_11AC_VHT160)) {
                            curr_phymode = IEEE80211_MODE_11AC_VHT160;
                        } else if ((cur_mode == IEEE80211_MODE_11AXA_HE80_80) ||
                            (cur_mode == IEEE80211_MODE_11AC_VHT80_80)) {
                            curr_phymode = IEEE80211_MODE_11AC_VHT80_80;
                        } else {
                            qdf_print("%s: Warning: Unexpected negotiated "
                            "ni_chwidth=%d for cur_mode=%d. Investigate! "
                            "The system may no longer function "
                            "correctly.",
                            __func__, ni->ni_chwidth, cur_mode);
                            return curr_phymode;
                        }
                    break;
                    default:
                        return curr_phymode;
                    break;
                }
            }
            break;
            /* HT mode */
            case 3:
            {
                switch (ni->ni_chwidth) {
                    case IEEE80211_CWM_WIDTH20:
                        curr_phymode = IEEE80211_MODE_11NA_HT20;
                    break;
                    case IEEE80211_CWM_WIDTH40:
                        curr_phymode = IEEE80211_MODE_11NA_HT40;
                    break;
                    default:
                        return curr_phymode;
                    break;
                }
            }
            break;
            default:
                return curr_phymode;
            break;
        }
    } else {
        switch (mode) {
            /* HE mode */
            case 1:
            {
                switch (ni->ni_chwidth) {
                    case IEEE80211_CWM_WIDTH20 :
                        curr_phymode = IEEE80211_MODE_11AXG_HE20;
                    break;
                    case IEEE80211_CWM_WIDTH40 :
                        curr_phymode = IEEE80211_MODE_11AXG_HE40;
                    break;
                    default:
                        return curr_phymode;
                    break;
                }
            }
            break;
            /* HT mode */
            case 3:
            {
                switch (ni->ni_chwidth) {
                    case IEEE80211_CWM_WIDTH20 :
                        curr_phymode = IEEE80211_MODE_11NG_HT20;
                    break;
                    case IEEE80211_CWM_WIDTH40 :
                        curr_phymode = IEEE80211_MODE_11NG_HT40;
                    break;
                    default:
                       return curr_phymode;
                    break;
                }
            }
            break;
            default:
                return curr_phymode;
            break;
        }
    }
    return curr_phymode;
}

void
get_sta_info(void *arg, wlan_node_t node)
{
    struct stainforeq *req = arg;
    wlan_if_t vap = req->vap;
    struct ieee80211req_sta_info *si;
    size_t ielen, len;
    u_int8_t *cp;
    u_int8_t    ni_ie[IEEE80211_MAX_OPT_IE];
    u_int16_t ni_ie_len = IEEE80211_MAX_OPT_IE;
    u_int8_t *macaddr = wlan_node_getmacaddr(node);
    wlan_rssi_info rssi_info;
    wlan_chan_t chan = wlan_node_get_chan(node);
    ieee80211_rate_info rinfo;
    u_int32_t jiffies_now=0, jiffies_delta=0, jiffies_assoc=0;
    uint32_t inactive_time;
    struct wlan_objmgr_psoc *psoc;
    struct cdp_peer_stats *peer_stats;
    u_int32_t op_class=0, op_rates=0;
    struct cdp_peer *peer_dp_handle;
    ol_txrx_soc_handle soc_dp_handle;
    struct wlan_objmgr_pdev *pdev;

    /* already ignore invalid nodes in UMAC */
    if (chan == IEEE80211_CHAN_ANYC) { /* XXX bogus entry */
        return;
    }
    if (!vap || !vap->iv_ic)
        return;

    pdev = vap->iv_ic->ic_pdev_obj;
    psoc = wlan_pdev_get_psoc(pdev);

    len = sta_space(node, &ielen, vap);
    if (len > req->space) {
        return;
    }

    si = req->si;
    si->awake_time = node->awake_time;
    si->ps_time = node->ps_time;
    /* if node state is currently in power save when the wlanconfig command is given,
       add time from previous_ps_time until current time to power save time */
    if(node->ps_state == 1)
    {
    si->ps_time += qdf_get_system_timestamp() - node->previous_ps_time;
    }
    /* if node state is currently in active state when the wlanconfig command is given,
       add time from previous_ps_time until current time to awake time */
    else if(node->ps_state == 0)
    {
    si->awake_time += qdf_get_system_timestamp() - node->previous_ps_time;
    }
    si->isi_assoc_time = wlan_node_get_assocuptime(node);
    jiffies_assoc = wlan_node_get_assocuptime(node);		/* Jiffies to timespec conversion for si->isi_tr069_assoc_time */
    jiffies_now = OS_GET_TICKS();
    jiffies_delta = jiffies_now - jiffies_assoc;
    jiffies_to_timespec(jiffies_delta, &si->isi_tr069_assoc_time);
    si->isi_len = len;
    si->isi_ie_len = ielen;
    si->isi_freq = wlan_channel_frequency(chan);
    if(!vap->iv_ic->ic_is_target_lithium){
        qdf_err("ic_is_target_lithium if null");
        return;
    }
    if(vap->iv_ic->ic_is_target_lithium(psoc)){
        if(!vap->iv_ic->ic_get_cur_hw_nf){
            qdf_err("ic_get_cur_hw_nf is null");
            return;
        }
        si->isi_nf = vap->iv_ic->ic_get_cur_hw_nf(vap->iv_ic);
    } else {
        if(!vap->iv_ic->ic_get_cur_chan_nf){
            qdf_err("ic_get_cur_hw_nf is null");
            return;
        }
        si->isi_nf = vap->iv_ic->ic_get_cur_chan_nf(vap->iv_ic);
    }
    si->isi_ieee = wlan_channel_ieee(chan);
    si->isi_flags = wlan_channel_flags(chan);
    si->isi_state = wlan_node_get_state_flag(node);
    if(vap->iv_ic->ic_is_mode_offload(vap->iv_ic)) {
        si->isi_ps = node->ps_state;
    } else {
        si->isi_ps = (si->isi_state & IEEE80211_NODE_PWR_MGT)?1:0;
    }
    si->isi_authmode =  wlan_node_get_authmode(node);
    if (wlan_node_getrssi(node, &rssi_info, WLAN_RSSI_RX) == 0) {
        si->isi_rssi = rssi_info.avg_rssi;
        si->isi_min_rssi = node->ni_rssi_min;
        si->isi_max_rssi = node->ni_rssi_max;
    }
    si->isi_capinfo = wlan_node_getcapinfo(node);
#if ATH_BAND_STEERING
    si->isi_pwrcapinfo = wlan_node_getpwrcapinfo(node);
#endif
    si->isi_athflags = wlan_node_get_ath_flags(node);
    si->isi_erp = wlan_node_get_erp(node);
    si->isi_operating_bands = wlan_node_get_operating_bands(node);
    si->isi_beacon_measurement_support = wlan_node_has_extflag(node, IEEE80211_NODE_BCN_MEASURE_SUPPORT);
    IEEE80211_ADDR_COPY(si->isi_macaddr, macaddr);

    if (wlan_node_txrate_info(node, &rinfo) == 0) {
        si->isi_txratekbps = rinfo.rate;
        si->isi_maxrate_per_client = rinfo.maxrate_per_client;
#if ATH_EXTRA_RATE_INFO_STA
        si->isi_tx_rate_mcs = rinfo.mcs;
        si->isi_tx_rate_flags = rinfo.flags;
#endif

    }

    /* supported operating classes */
    if (node->ni_supp_op_class_ie != NULL) {
        si->isi_curr_op_class = node->ni_supp_op_cl.curr_op_class;
        si->isi_num_of_supp_class = node->ni_supp_op_cl.num_of_supp_class;
        for(op_class = 0; op_class < node->ni_supp_op_cl.num_of_supp_class &&
            op_class < MAX_NUM_OPCLASS_SUPPORTED; op_class++) {
            si->isi_supp_class[op_class] = node->ni_supp_op_cl.supp_class[op_class];
        }
    }
    else {
          si->isi_num_of_supp_class = 0;
    }

    /* supported channels */
    if (node->ni_supp_chan_ie != NULL) {
        si->isi_first_channel = node->ni_first_channel;
        si->isi_nr_channels = node->ni_nr_channels;
    }
    else {
         si->isi_nr_channels = 0;
    }

    /* supported rates */
    for (op_rates = 0;op_rates < node->ni_rates.rs_nrates;op_rates++) {
         si->isi_rates[op_rates] = node->ni_rates.rs_rates[op_rates];
    }

    memset(&rinfo, 0, sizeof(rinfo));
    if (wlan_node_rxrate_info(node, &rinfo) == 0) {
        si->isi_rxratekbps = rinfo.rate;
#if ATH_EXTRA_RATE_INFO_STA
        si->isi_rx_rate_mcs = rinfo.mcs;
        si->isi_rx_rate_flags = rinfo.flags;
#endif

    }
    si->isi_associd = wlan_node_get_associd(node);
    si->isi_txpower = wlan_node_get_txpower(node);
    si->isi_vlan = wlan_node_get_vlan(node);
    si->isi_cipher = IEEE80211_CIPHER_NONE;
    if (wlan_get_param(vap, IEEE80211_FEATURE_PRIVACY)) {
        do {
            ieee80211_cipher_type uciphers[1];
            int count = 0;
            count = wlan_node_get_ucast_ciphers(node, uciphers, 1);
            if (count == 1) {
                si->isi_cipher |= 1<<uciphers[0];
            }
        } while (0);
    }
    wlan_node_get_txseqs(node, si->isi_txseqs, sizeof(si->isi_txseqs));
    wlan_node_get_rxseqs(node, si->isi_rxseqs, sizeof(si->isi_rxseqs));
    si->isi_uapsd = wlan_node_get_uapsd(node);
    si->isi_opmode = IEEE80211_STA_OPMODE_NORMAL;

    peer_dp_handle = wlan_peer_get_dp_handle(node->peer_obj);
    if (!peer_dp_handle)
        return;

    soc_dp_handle = wlan_psoc_get_dp_handle(psoc);
    if (!soc_dp_handle)
        return;

    peer_stats = cdp_host_get_peer_stats(soc_dp_handle,
                                         peer_dp_handle);
    if (!peer_stats)
        return;

    inactive_time = peer_stats->tx.inactive_time;
    if(vap->iv_ic->ic_is_mode_offload(vap->iv_ic))
        si->isi_inact = inactive_time;
    else
        si->isi_inact = wlan_node_get_inact(node);
    /* 11n */
    si->isi_htcap = wlan_node_get_htcap(node);
    si->isi_stamode= wlan_node_get_mode(node);
    si->isi_curr_mode = get_phymode_from_chwidth(vap->iv_ic, node);

#if ATH_SUPPORT_EXT_STAT
    si->isi_vhtcap = wlan_node_get_vhtcap(node);
    si->isi_chwidth = (u_int8_t) wlan_node_get_chwidth(node);
#endif

    /* Extended capabilities */
    si->isi_ext_cap = wlan_node_get_extended_capabilities(node);
    si->isi_ext_cap2 = wlan_node_get_extended_capabilities2(node);
    si->isi_ext_cap3 = wlan_node_get_extended_capabilities3(node);
    si->isi_ext_cap4 = wlan_node_get_extended_capabilities4(node);
    si->isi_nss = wlan_node_get_nss(node);
    si->isi_is_256qam = wlan_node_get_256qam_support(node);

    cp = (u_int8_t *)(si+1);

    if(!wlan_node_getwpaie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getwmeie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getathie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }
    if(!wlan_node_getwpsie(vap, macaddr, ni_ie, &ni_ie_len)) {
        OS_MEMCPY(cp, ni_ie, ni_ie_len);
        cp += ni_ie_len;
        ni_ie_len = IEEE80211_MAX_OPT_IE;
    }

    req->si = (
    struct ieee80211req_sta_info *)(((u_int8_t *)si) + len);
    req->space -= len;
}
int ieee80211_ucfg_getstaspace(wlan_if_t vap)
{
    struct stainforeq req;


    /* estimate space required for station info */
    req.space = sizeof(struct stainforeq);
    req.vap = vap;
    wlan_iterate_station_list(vap, get_sta_space, &req);

    return req.space;

}
int ieee80211_ucfg_getstainfo(wlan_if_t vap, struct ieee80211req_sta_info *si, uint32_t *len)
{
    struct stainforeq req;


    if (*len < sizeof(struct ieee80211req_sta_info))
        return -EFAULT;

    /* estimate space required for station info */
    req.space = sizeof(struct stainforeq);
    req.vap = vap;

    if (*len > 0)
    {
        size_t space = *len;

        if (si == NULL)
            return -ENOMEM;

        req.si = si;
        req.space = *len;

        wlan_iterate_station_list(vap, get_sta_info, &req);
        *len = space - req.space;
    }
    else
        *len = 0;

    return 0;
}

#if ATH_SUPPORT_IQUE
int ieee80211_ucfg_rcparams_setrtparams(wlan_if_t vap, uint8_t rt_index, uint8_t per, uint8_t probe_intvl)
{
    if ((rt_index != 0 && rt_index != 1) || per > 100 ||
        probe_intvl > 100)
    {
        goto error;
    }
    wlan_set_rtparams(vap, rt_index, per, probe_intvl);
    return 0;

error:
    qdf_print("usage: rtparams rt_idx <0|1> per <0..100> probe_intval <0..100>");
    return -EINVAL;
}

int ieee80211_ucfg_rcparams_setratemask(wlan_if_t vap, uint8_t preamble,
        uint32_t mask_lower32, uint32_t mask_higher32, uint32_t mask_lower32_2)
{
    osif_dev *osifp = (osif_dev *)vap->iv_ifp;
    struct net_device *dev = osifp->netdev;
    struct ieee80211com *ic = vap->iv_ic;
    int retv = -EINVAL;

    if(!osifp->osif_is_mode_offload) {
        qdf_print("This command is only supported on offload case!");
    } else {
        switch(preamble)
        {
            case IEEE80211_LEGACY_PREAMBLE:
                if ((mask_lower32 > 0xFFF) ||
                       (mask_higher32 != 0) ||
                       (mask_lower32_2 != 0)) {
                    qdf_print("Invalid ratemask for CCK/OFDM");
                    return retv;
                } else {
                    break;
                }
            case IEEE80211_HT_PREAMBLE:
                if((mask_higher32 != 0) || (mask_lower32_2 != 0)) {
                    qdf_print("Invalid ratemask for HT");
                    return retv;
                } else {
                    break;
                }
            case IEEE80211_VHT_PREAMBLE:
                /* For HE targets, we now support MCS0-11 for upto NSS 8 for VHT.
                 * But for legacy targets we have support till MCS0-9 NSS 4.
                 * Hence the below check ensures the correct bitmask is sent
                 * depending on the target. */
                if (!(ic->ic_he_target) && ((mask_higher32 > 0xFF) ||
                                            (mask_lower32_2 != 0))) {
                    qdf_print("Invalid ratemask for VHT");
                    return retv;
                } else {
                    break;
                }
            case IEEE80211_HE_PREAMBLE:
                if(!ic->ic_he_target){
                    qdf_print("HE preamble not supported for this target.");
                    return retv;
                } else {
                    break;
                }
            default:
                qdf_print("Invalid preamble type");
                return retv;
        }
        retv = ic->ic_vap_set_ratemask(vap, preamble, mask_lower32,
                mask_higher32, mask_lower32_2);
        if (retv == ENETRESET) {
            retv = IS_UP(dev) ? osif_vap_init(dev, RESCAN) : 0;
        }
    }
    return retv;
}
#endif

static int
is_null_mac(char *addr)
{
	char nullmac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	if (IEEE80211_ADDR_EQ(addr, nullmac))
		return 1;

	return 0;
}

#if WLAN_CFR_ENABLE
int
ieee80211_ucfg_cfr_params(struct ieee80211com *ic, wlan_if_t vap, struct ieee80211_wlanconfig *config)
{
    struct ieee80211_wlanconfig_cfr *cfr_params;
    struct ieee80211_node *ni = NULL;
    int retv = 0;
    struct cfr_capture_params cfr_req = {0};
    struct wlan_objmgr_pdev *pdev = NULL;

    cfr_params = &config->data.cfr_config;

    switch (config->cmdtype) {

        case IEEE80211_WLANCONFIG_CFR_START:
            ni = ieee80211_vap_find_node(vap, &(cfr_params->mac[0]));
            if (ni == NULL) {
                qdf_info("Peer not found \n");
                return -EINVAL;
            }

            if (cfr_params->bandwidth > ni->ni_chwidth) {
                qdf_info("Invalid bandwidth\n");
                ieee80211_free_node(ni);
                return -EINVAL;
            }
	    if (cfr_params->capture_method >= CFR_CAPTURE_METHOD_MAX) {
                qdf_info("Invalid capture method\n");
                ieee80211_free_node(ni);
                return -EINVAL;
            }

            pdev = vap->iv_ic->ic_pdev_obj;
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_CFR_ID) !=
                    QDF_STATUS_SUCCESS) {
                ieee80211_free_node(ni);
                return -EINVAL;
            }

            cfr_req.period = cfr_params->periodicity;
            cfr_req.bandwidth = cfr_params->bandwidth;
            cfr_req.method = cfr_params->capture_method;
            retv = ucfg_cfr_start_capture(pdev, ni->peer_obj, &cfr_req);

            ieee80211_free_node(ni);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

            break;
        case IEEE80211_WLANCONFIG_CFR_STOP:
            ni = ieee80211_vap_find_node(vap, &(cfr_params->mac[0]));
            if (ni == NULL) {
                qdf_info("Peer not found \n");
                return -EINVAL;
            }

            pdev = vap->iv_ic->ic_pdev_obj;
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_CFR_ID) !=
                    QDF_STATUS_SUCCESS) {
                ieee80211_free_node(ni);
                return -EINVAL;
            }

            retv = ucfg_cfr_stop_capture(pdev, ni->peer_obj);

            ieee80211_free_node(ni);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);

            break;
        case IEEE80211_WLANCONFIG_CFR_LIST_PEERS:
            break;
        default:
            qdf_info("Invalid CFR command: %d \n", config->cmdtype);
            return -EIO;
    }
    return retv;
}
#endif

int
ieee80211_ucfg_nawds(wlan_if_t vap, struct ieee80211_wlanconfig *config)
{
	struct ieee80211_wlanconfig_nawds *nawds;
	nawds = &config->data.nawds;
	switch (config->cmdtype) {
		case IEEE80211_WLANCONFIG_NAWDS_SET_MODE:
			return wlan_nawds_set_param(vap, IEEE80211_NAWDS_PARAM_MODE, &nawds->mode);
		case IEEE80211_WLANCONFIG_NAWDS_SET_DEFCAPS:
			return wlan_nawds_set_param(vap, IEEE80211_NAWDS_PARAM_DEFCAPS, &nawds->defcaps);
		case IEEE80211_WLANCONFIG_NAWDS_SET_OVERRIDE:
			return wlan_nawds_set_param(vap, IEEE80211_NAWDS_PARAM_OVERRIDE, &nawds->override);
		case IEEE80211_WLANCONFIG_NAWDS_SET_ADDR:
			{
				int status;
				status = wlan_nawds_config_mac(vap, nawds->mac, nawds->caps);
				if( status == 0 ) {
					OS_SLEEP(250000);
				}
				return status;
			}
		case IEEE80211_WLANCONFIG_NAWDS_KEY:
			memcpy(vap->iv_nawds.psk, nawds->psk, strlen(nawds->psk));
			if (is_null_mac(nawds->mac)) {
				qdf_info("mac is NULL, psk set for learning RE\n");
				return 0;
			}
			return wlan_nawds_config_key(vap, nawds->mac, nawds->psk);
		case IEEE80211_WLANCONFIG_NAWDS_CLR_ADDR:
			return wlan_nawds_delete_mac(vap, nawds->mac);
		case IEEE80211_WLANCONFIG_NAWDS_GET:
			wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_MODE, &nawds->mode);
			wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_DEFCAPS, &nawds->defcaps);
			wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_OVERRIDE, &nawds->override);
			if (wlan_nawds_get_mac(vap, nawds->num, &nawds->mac[0], &nawds->caps)) {
				qdf_print("failed to get NAWDS entry %d", nawds->num);
			}
			wlan_nawds_get_param(vap, IEEE80211_NAWDS_PARAM_NUM, &nawds->num);
			config->status = IEEE80211_WLANCONFIG_OK;
			break;
		default :
			return -EIO;
	}
	return 0;
}

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
static int
ieee80211_hmmc_dump(struct ieee80211com *ic)
{
	int i;

	qdf_print("\nMULTICAST RANGE:");
	for (i = 0; i < ic->ic_hmmc_cnt; i++)
		qdf_print("\t%d of %d: %08x/%08x",
				i+1,
				ic->ic_hmmc_cnt,
				ic->ic_hmmcs[i].ip,
				ic->ic_hmmcs[i].mask);
	return 0;
}
int
ieee80211_ucfg_hmmc(wlan_if_t vap, struct ieee80211_wlanconfig *config)
{
	struct ieee80211_wlanconfig_hmmc *hmmc;
	u_int8_t ret;
	struct ieee80211com *ic = vap->iv_ic;
	hmmc = &(config->data.hmmc);
	switch (config->cmdtype) {
		case IEEE80211_WLANCONFIG_HMMC_ADD:
			ret = ieee80211_add_hmmc(vap, hmmc->ip, hmmc->mask);
			break;
		case IEEE80211_WLANCONFIG_HMMC_DEL:
			ret = ieee80211_del_hmmc(vap, hmmc->ip, hmmc->mask);
			break;
		case IEEE80211_WLANCONFIG_HMMC_DUMP:
			ret = ieee80211_hmmc_dump(ic);
			break;
		default:
			ret = -EOPNOTSUPP;
	}

	return 0;
}
#endif
int
ieee80211_ucfg_ald(wlan_if_t vap, struct ieee80211_wlanconfig *config)
{
    struct ieee80211_wlanconfig_ald *config_ald;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    u_int8_t ret = 0;
#endif
    config_ald = &(config->data.ald);
    switch (config->cmdtype) {
    #if ATH_SUPPORT_HYFI_ENHANCEMENTS
        case IEEE80211_WLANCONFIG_ALD_STA_ENABLE:
            ret = wlan_ald_sta_enable(vap, config_ald->data.ald_sta.macaddr, config_ald->data.ald_sta.enable);
            break;
     #endif
        default:
            OS_FREE(config);
            return -ENXIO;
    }

    return 0;
}
int
ieee80211_ucfg_hmwds(wlan_if_t vap, struct ieee80211_wlanconfig *config, int buffer_len)
{
    struct ieee80211_wlanconfig_hmwds *hmwds;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    struct ieee80211_wlanconfig_wds_table *wds_table;
#endif
    int ret = 0;
    hmwds = &(config->data.hmwds);
    switch (config->cmdtype) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        case IEEE80211_WLANCONFIG_HMWDS_ADD_ADDR:
            ret = wlan_hmwds_add_addr(vap, hmwds->wds_ni_macaddr, hmwds->wds_macaddr, hmwds->wds_macaddr_cnt);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_RESET_ADDR:
            ret = wlan_hmwds_reset_addr(vap, hmwds->wds_ni_macaddr);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_RESET_TABLE:
            ret = wlan_hmwds_reset_table(vap);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_READ_ADDR:
            hmwds->wds_macaddr_cnt = buffer_len - sizeof (*config);
            ret = wlan_hmwds_read_addr(vap, hmwds->wds_ni_macaddr, hmwds->wds_macaddr, &hmwds->wds_macaddr_cnt);
            if (ret)
                hmwds->wds_macaddr_cnt = 0;
            break;
        case IEEE80211_WLANCONFIG_HMWDS_READ_TABLE:
            wds_table = &config->data.wds_table;
            wds_table->wds_entry_cnt = buffer_len - sizeof (*config);
            ret = wlan_wds_read_table(vap, wds_table);
            if (ret)
                wds_table->wds_entry_cnt = 0;
            break;
        case IEEE80211_WLANCONFIG_HMWDS_SET_BRIDGE_ADDR:
            ret = wlan_hmwds_set_bridge_mac_addr(vap, hmwds->wds_macaddr);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_REMOVE_ADDR:
            ret = wlan_hmwds_remove_addr(vap, hmwds->wds_macaddr);
            break;
        case IEEE80211_WLANCONFIG_HMWDS_DUMP_WDS_ADDR:
            ret = wlan_wds_dump_wds_addr(vap);
            break;
#endif
        default:
            return -ENXIO;
    }
    return ret;
}

#if UMAC_SUPPORT_WNM
int
ieee80211_ucfg_wnm(wlan_if_t vap, struct ieee80211_wlanconfig *config)
{
	struct ieee80211_wlanconfig_wnm *wnm;
	struct ieee80211com *ic = vap->iv_ic;
	int status = 0;
	osif_dev *osifp = (osif_dev *)vap->iv_ifp;
	struct net_device *dev = osifp->netdev;

	if (!wlan_wnm_vap_is_set(vap))
		return -EFAULT;

	wnm = &(config->data.wnm);
	switch (config->cmdtype) {
		case IEEE80211_WLANCONFIG_WNM_SET_BSSMAX:
			status = wlan_wnm_set_bssmax(vap, &wnm->data.bssmax);
			if (status == 0) {
				status = IS_UP(dev) ? -osif_vap_init(dev, RESCAN) : 0;
			} else {
				return -EFAULT;
			}
			break;
		case IEEE80211_WLANCONFIG_WNM_GET_BSSMAX:
			status = wlan_wnm_get_bssmax(vap, &wnm->data.bssmax);
			config->status = (status == 0) ? IEEE80211_WLANCONFIG_OK : IEEE80211_WLANCONFIG_FAIL;
			break;
		case IEEE80211_WLANCONFIG_WNM_TFS_ADD:
			status = wlan_wnm_set_tfs(vap, &wnm->data.tfs);
			return status;

		case IEEE80211_WLANCONFIG_WNM_TFS_DELETE:
			/* since there is no tfs request elements its send the
			   TFS requestion action frame with NULL elements which
			   will delete the existing request on AP as per specification */
			status = wlan_wnm_set_tfs(vap, &wnm->data.tfs);
			return status;

		case IEEE80211_WLANCONFIG_WNM_FMS_ADD_MODIFY:
			status = wlan_wnm_set_fms(vap, &wnm->data.fms);
			return status;

		case IEEE80211_WLANCONFIG_WNM_SET_TIMBCAST:
			status = wlan_wnm_set_timbcast(vap, &wnm->data.tim);
			return status;

		case IEEE80211_WLANCONFIG_WNM_GET_TIMBCAST:
			status = wlan_wnm_get_timbcast(vap, &wnm->data.tim);
			config->status = (status == 0) ? IEEE80211_WLANCONFIG_OK : IEEE80211_WLANCONFIG_FAIL;
			break;

		case IEEE80211_WLANCONFIG_WNM_BSS_TERMINATION:

			/*
			 * For offload Architecture we have no way to get the MAC TSF as of now.
			 * Disabling this feature for offload untill we have a way to
			 * get the TSF.
			 */
			if (ic->ic_is_mode_offload(ic)) {
				qdf_print("Disabled for Offload Architecture");
				return -EINVAL;
			}
			status = wlan_wnm_set_bssterm(vap, &wnm->data.bssterm);
			return status;

		default:
			break;
	}
	return 0;
}
#endif

int ieee80211_ucfg_addie(wlan_if_t vap, struct ieee80211_wlanconfig_ie *ie_buffer)
{
    int error = 0;

    switch (ie_buffer->ftype) {
        case IEEE80211_APPIE_FRAME_BEACON:
            wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_BEACON, (u_int8_t*)&ie_buffer->ie, ie_buffer->ie.len, ie_buffer->ie.elem_id);
            break;
        case IEEE80211_APPIE_FRAME_PROBE_RESP:
            wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_PROBERESP, (u_int8_t*)&ie_buffer->ie, ie_buffer->ie.len, ie_buffer->ie.elem_id);
            break;
        case IEEE80211_APPIE_FRAME_ASSOC_RESP:
            wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_ASSOCRESP, (u_int8_t*)&ie_buffer->ie, ie_buffer->ie.len, ie_buffer->ie.elem_id);
            break;
        default:
            QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR, "Frame type not supported\n");
            error = -ENXIO;
    }

    return error;
}

#if ATH_SUPPORT_DYNAMIC_VENDOR_IE
int
ieee80211_ucfg_vendorie(wlan_if_t vap, struct ieee80211_wlanconfig_vendorie *vie)
{
	int error;
	switch (vie->cmdtype) {
		case IEEE80211_WLANCONFIG_VENDOR_IE_ADD:
			error = wlan_set_vendorie(vap, IEEE80211_VENDOR_IE_PARAM_ADD, vie);
			break;

		case IEEE80211_WLANCONFIG_VENDOR_IE_UPDATE:
			error = wlan_set_vendorie(vap, IEEE80211_VENDOR_IE_PARAM_UPDATE, vie);
			break;

		case IEEE80211_WLANCONFIG_VENDOR_IE_REMOVE:
			error = wlan_set_vendorie(vap, IEEE80211_VENDOR_IE_PARAM_REMOVE, vie);
			break;

		default:
			error = -ENXIO;
	}
	return error;
}
#endif
#if ATH_SUPPORT_NAC_RSSI
int
ieee80211_ucfg_nac_rssi(wlan_if_t vap, struct ieee80211_wlanconfig *config)
{
    struct ieee80211_wlanconfig_nac_rssi *nac_rssi;
    nac_rssi = &(config->data.nac_rssi);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"%s:cmd_type=%d BSSID:%2x:%2x:%2x:%2x:%2x:%2x, "
        "client:%2x:%2x:%2x:%2x:%2x:%2x channel: %d\n", __func__,
         config->cmdtype, nac_rssi->mac_bssid[0], nac_rssi->mac_bssid[1],
         nac_rssi->mac_bssid[2], nac_rssi->mac_bssid[3], nac_rssi->mac_bssid[4],
         nac_rssi->mac_bssid[5], nac_rssi->mac_client[0], nac_rssi->mac_client[1],
         nac_rssi->mac_client[2], nac_rssi->mac_client[3], nac_rssi->mac_client[4],
         nac_rssi->mac_client[5], nac_rssi->chan_num);

    switch (config->cmdtype) {
        case IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_ADD:
            return wlan_set_nac_rssi(vap, IEEE80211_NAC_RSSI_PARAM_ADD, nac_rssi);

        case IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_DEL:
            return wlan_set_nac_rssi(vap, IEEE80211_NAC_RSSI_PARAM_DEL, nac_rssi);

        case IEEE80211_WLANCONFIG_NAC_RSSI_ADDR_LIST:
            return wlan_list_nac_rssi(vap, IEEE80211_NAC_RSSI_PARAM_LIST, nac_rssi);

        default:
            return -ENXIO;
   }
}

#endif
#if ATH_SUPPORT_NAC
int
ieee80211_ucfg_nac(wlan_if_t vap, struct ieee80211_wlanconfig *config)
{
	struct ieee80211_wlanconfig_nac *nac;
	nac = &(config->data.nac);

	switch (config->cmdtype) {
		case IEEE80211_WLANCONFIG_NAC_ADDR_ADD:
			return wlan_set_nac(vap, IEEE80211_NAC_PARAM_ADD, nac);

		case IEEE80211_WLANCONFIG_NAC_ADDR_DEL:
			return wlan_set_nac(vap, IEEE80211_NAC_PARAM_DEL, nac);

		case IEEE80211_WLANCONFIG_NAC_ADDR_LIST:
			wlan_list_nac(vap, IEEE80211_NAC_PARAM_LIST, nac);
			break;

		default:
			return -ENXIO;
	}
	return 0;

}
#endif

int ieee80211_ucfg_scanlist(wlan_if_t vap)
{
	int retv = 0;
	osif_dev *osifp = (osif_dev *)vap->iv_ifp;
	struct scan_start_request *scan_params = NULL;
	int time_elapsed = OS_SIWSCAN_TIMEOUT;
	bool scan_pause;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev = NULL;

	vdev = osifp->ctrl_vdev;
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		scan_info("unable to get reference");
		return -EBUSY;
	}
	/* Increase timeout value for EIR since a rpt scan itself takes 12 seconds */
	if (ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic))
		time_elapsed = OS_SIWSCAN_TIMEOUT * SIWSCAN_TIME_ENH_IND_RPT;
	scan_pause =  (ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic)) ?(vap->iv_pause_scan  ) : 0;
	/* start a scan */
	if ((time_after(OS_GET_TICKS(), osifp->os_last_siwscan + time_elapsed))
			&& (osifp->os_giwscan_count == 0) &&
			(vap->iv_opmode == IEEE80211_M_STA)&& !scan_pause) {
		if (ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic)) {
			scan_params = qdf_mem_malloc(sizeof(*scan_params));
			if (!scan_params) {
				wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
				return -ENOMEM;
			}
			status = wlan_update_scan_params(vap, scan_params, wlan_vap_get_opmode(vap), true, true, true, true, 0, NULL, 0);
			if (status) {
				wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
				scan_err("scan param init failed with status: %d", status);
				qdf_mem_free(scan_params);
				return -EINVAL;
			}
			scan_params->scan_req.scan_f_forced = true;
			scan_params->scan_req.min_rest_time = MIN_REST_TIME;
			scan_params->scan_req.max_rest_time = MAX_REST_TIME;
			scan_params->legacy_params.min_dwell_active = MIN_DWELL_TIME_ACTIVE;
			scan_params->scan_req.dwell_time_active = MAX_DWELL_TIME_ACTIVE;
			scan_params->legacy_params.scan_type = SCAN_TYPE_BACKGROUND;
			scan_params->legacy_params.init_rest_time = INIT_REST_TIME;
			osifp->os_last_siwscan = OS_GET_TICKS();

			status = wlan_ucfg_scan_start(vap, scan_params, osifp->scan_requestor,
					SCAN_PRIORITY_LOW, &(osifp->scan_id), 0, NULL);
			if (status) {
				scan_err("scan_start failed with status: %d", status);
			}
		}
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
	osifp->os_giwscan_count = 0;
	return retv;
}

size_t
scan_space(wlan_scan_entry_t se, u_int16_t *ielen)
{
	u_int8_t ssid_len;
	uint8_t ssid_ie_len = 0;

	*ielen = 0;
	ssid_len = util_scan_entry_ssid(se)->length;
	*ielen =  util_scan_entry_ie_len(se);
	if (util_scan_entry_is_hidden_ap(se) && ssid_len) {
		ssid_ie_len = ssid_len + (sizeof(uint8_t) * 2);
	}
	return roundup(sizeof(struct ieee80211req_scan_result) +
			*ielen +  ssid_len + ssid_ie_len, sizeof(u_int32_t));
}

QDF_STATUS
get_scan_space(void *arg, wlan_scan_entry_t se)
{
	struct scanreq *req = arg;
	u_int16_t ielen;

	req->space += scan_space(se, &ielen);
	return 0;
}

QDF_STATUS
get_scan_space_rep_move(void *arg, wlan_scan_entry_t se)
{
	struct scanreq *req = arg;
	struct ieee80211vap *vap = req->vap;
	u_int16_t ielen;
	u_int8_t *ssid;
	u_int8_t ssid_len;
	u_int8_t *bssid;

	ssid_len = util_scan_entry_ssid(se)->length;
	ssid = util_scan_entry_ssid(se)->ssid;
	bssid = util_scan_entry_bssid(se);

	/* Calculate scan space only for those scan entries that match
	 * the SSID of the new Root AP
	 */
	if (ssid) {
	    if (!(strncmp(ssid, vap->iv_ic->ic_repeater_move.ssid.ssid,
	                  vap->iv_ic->ic_repeater_move.ssid.len))) {
		if(IEEE80211_ADDR_IS_VALID(vap->iv_ic->ic_repeater_move.bssid)) {
		    if (IEEE80211_ADDR_EQ(bssid, vap->iv_ic->ic_repeater_move.bssid)) {
		        req->space += scan_space(se, &ielen);
		    }
		} else {
	            req->space += scan_space(se, &ielen);
	        }
	    }
	}

	return 0;
}

QDF_STATUS
get_scan_result(void *arg, wlan_scan_entry_t se)
{
	struct scanreq *req = arg;
	struct ieee80211req_scan_result *sr;
	u_int16_t ielen, len, nr, nxr;
	u_int8_t *cp;
	u_int8_t ssid_len;
        struct ieee80211vap *vap = req->vap;
        bool des_ssid_found = 0;
	u_int8_t *rates, *ssid, *bssid;

	len = scan_space(se, &ielen);
	if (len > req->space)
		return 0;

	sr = req->sr;
	memset(sr, 0, sizeof(*sr));
	ssid_len = util_scan_entry_ssid(se)->length;
	ssid = util_scan_entry_ssid(se)->ssid;
	bssid = util_scan_entry_bssid(se);
	sr->isr_ssid_len = ssid_len;

        if (req->scanreq_type == SCANREQ_GIVE_ONLY_DESSIRED_SSID) {
            des_ssid_found = ieee80211_vap_match_ssid((struct ieee80211vap *) vap->iv_ic->ic_sta_vap, ssid, ssid_len);
            if (!des_ssid_found) {
                return 0;
            } else {
                qdf_print("Getting desired ssid from scan result");
            }
        }
        if (req->scanreq_type == SCANREQ_GIVE_EXCEPT_DESSIRED_SSID) {
            des_ssid_found = ieee80211_vap_match_ssid((struct ieee80211vap *) vap->iv_ic->ic_sta_vap, ssid, ssid_len);
            if (des_ssid_found) {
                return 0;
            }
        }

	if (vap->iv_ic->ic_repeater_move.state == REPEATER_MOVE_IN_PROGRESS) {
	    if (ssid == NULL)
	        return 0;

	    if (bssid == NULL)
	        return 0;

	    if ((strncmp(ssid, vap->iv_ic->ic_repeater_move.ssid.ssid,
	                 vap->iv_ic->ic_repeater_move.ssid.len)) != 0) {
	        return 0;
	    }

	    if(IEEE80211_ADDR_IS_VALID(vap->iv_ic->ic_repeater_move.bssid)) {
		if (!IEEE80211_ADDR_EQ(bssid, vap->iv_ic->ic_repeater_move.bssid))
		    return 0;
	    }
	}

	if (ielen > 65534 ) {
		ielen = 0;
	}
	sr->isr_ie_len = ielen;
	sr->isr_len = len;
	sr->isr_freq = wlan_channel_frequency(wlan_util_scan_entry_channel(se));
	sr->isr_flags = 0;
	sr->isr_rssi = util_scan_entry_rssi(se);
	sr->isr_intval =  util_scan_entry_beacon_interval(se);
	sr->isr_capinfo = util_scan_entry_capinfo(se).value;
	sr->isr_erp =  util_scan_entry_erpinfo(se);
	IEEE80211_ADDR_COPY(sr->isr_bssid, util_scan_entry_bssid(se));
	rates = util_scan_entry_rates(se);
	nr = 0;
	if (rates) {
		nr = min((int)rates[1], IEEE80211_RATE_MAXSIZE);
		memcpy(sr->isr_rates, rates+2, nr);
	}

	rates = util_scan_entry_xrates(se);
	nxr=0;
	if (rates) {
		nxr = min((int)rates[1], IEEE80211_RATE_MAXSIZE - nr);
		memcpy(sr->isr_rates+nr, rates+2, nxr);
	}
	sr->isr_nrates = nr + nxr;

	cp = (u_int8_t *)(sr+1);
	if (ssid) {
		memcpy(cp,ssid, sr->isr_ssid_len);
	}
	cp += sr->isr_ssid_len;

	/* If AP is hidden, insert SSID IE at front of IE list */
	if (util_scan_entry_is_hidden_ap(se) && ssid_len) {
		*cp++ = WLAN_ELEMID_SSID;
		*cp++ = ssid_len;
		qdf_mem_copy(cp, ssid, ssid_len);
		cp += ssid_len;
	}

	if (ielen) {
		util_scan_entry_copy_ie_data(se,cp,&ielen);
		cp += ielen;
	}

	req->space -= len;
	req->sr = (struct ieee80211req_scan_result *)(((u_int8_t *)sr) + len);

	return 0;
}

void ieee80211_ucfg_setmaxrate_per_client(void *arg, wlan_node_t node)
{
	struct ieee80211_wlanconfig_setmaxrate *smr =
		(struct ieee80211_wlanconfig_setmaxrate *)arg;
	struct ieee80211_node *ni = node;
	struct ieee80211com *ic = ni->ni_ic;
	int i, rate_updated = 0;

	if (IEEE80211_ADDR_EQ(ni->ni_macaddr, smr->mac)) {
		ni->ni_maxrate = smr->maxrate;
		if (ni->ni_maxrate == 0xff) {
			ni->ni_rates.rs_nrates = ni->ni_maxrate_legacy;
			ni->ni_htrates.rs_nrates = ni->ni_maxrate_ht;
			/* set the default vht max rate info */
			ni->ni_maxrate_vht = 0xff;
			rate_updated = 1;
			goto end;
		}
		/* legacy rate */
		if (!(ni->ni_maxrate & 0x80)) {

			/* For VHT/HT capable station, do not allow user to set legacy rate as max rate */
			if ((ni->ni_vhtcap) || (ni->ni_htcap))
				return;

			for (i = 0; i < ni->ni_rates.rs_nrates; i++) {
				if ((ni->ni_maxrate & IEEE80211_RATE_VAL)
						<= (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL))
				{
					rate_updated = 1;
					ni->ni_rates.rs_nrates = i + 1;
					ni->ni_htrates.rs_nrates = 0;
					ni->ni_maxrate_vht = 0;
					break;
				}
			}
		}
		/* HT rate */
		else if (ni->ni_maxrate < 0xc0) {
			if (!ni->ni_htcap) {
				return;
			}
			/* For VHT capable station, do not allow user to set HT rate as max rate */
			if (ni->ni_vhtcap) {
				return;
			}
			for (i = 0; i < ni->ni_htrates.rs_nrates; i++) {
				if ((ni->ni_maxrate & 0x7f) <= ni->ni_htrates.rs_rates[i]) {
					rate_updated = 1;
					ni->ni_htrates.rs_nrates = i + 1;
					ni->ni_maxrate_vht = 0;
					break;
				}
			}
		}
		/* VHT rate */
		else if (ni->ni_maxrate >= 0xc0 && ni->ni_maxrate <= 0xf9) {
#define VHT_MAXRATE_IDX_MASK    0x0F
#define VHT_MAXRATE_IDX_SHIFT   4
			u_int8_t maxrate_vht_idx = (ni->ni_maxrate & VHT_MAXRATE_IDX_MASK) + 1;
			u_int8_t maxrate_vht_stream = (((ni->ni_maxrate & ~VHT_MAXRATE_IDX_MASK) - 0xc0)
					>> VHT_MAXRATE_IDX_SHIFT) + 1;
			if (!ni->ni_vhtcap)
				return;
			/* b0-b3: vht rate idx; b4-b7: # stream */
			ni->ni_maxrate_vht = (maxrate_vht_stream << VHT_MAXRATE_IDX_SHIFT) | maxrate_vht_idx;
			rate_updated = 1;
#undef VHT_MAXRATE_IDX_MASK
#undef VHT_MAXRATE_IDX_SHIFT
		}
		else {
			qdf_print("Unknown max rate 0x%x", ni->ni_maxrate);
			return;
		}

end:
		/* Calling ath_net80211_rate_node_update() for Updating the node rate */
		if (rate_updated) {
			ic->ic_rate_node_update(ic, ni, 0);
		}

		if (ni->ni_maxrate == 0xff) {
			qdf_print("rateset initialized to negotiated rates");
		}
	}
}

int ieee80211_ucfg_set_peer_nexthop(void *osif, uint8_t *mac, uint32_t if_num)
{
    int retv = 0;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    osif_dev *osifp = (osif_dev *)osif;
    wlan_if_t vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vdev_set_peer_next_hop(osifp, mac, if_num);
        retv = 1;
    }

    return retv;
#else
    qdf_print("Setting peer nexthop is only supported in NSS Offload mode");
    return retv;
#endif
}

int ieee80211_ucfg_set_vlan_type(void *osif, uint8_t default_vlan, uint8_t port_vlan)
{
    int retv = 0;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    osif_dev *osifp = (osif_dev *)osif;
    wlan_if_t vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vdev_set_vlan_type(osifp, default_vlan, port_vlan);
        retv = 1;
    }

#endif
    return retv;
}
int ieee80211_ucfg_setwmmparams(void *osif, int wmmparam, int ac, int bss, int value)
{
    int retv = 0;
    osif_dev *osifp = (osif_dev *)osif;
    wlan_if_t vap = osifp->os_if;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ieee80211com *ic = vap->iv_ic;
#endif

    switch (wmmparam)
    {
        case IEEE80211_WMMPARAMS_CWMIN:
            if (value < 0 ||  value > 15) {
                retv = -EINVAL;
            }
            else {
                retv = wlan_set_wmm_param(vap, WLAN_WME_CWMIN,
                        bss, ac, value);
            }
            break;
        case IEEE80211_WMMPARAMS_CWMAX:
            if (value < 0 ||  value > 15) {
                retv = -EINVAL;
            }
            else {
                retv = wlan_set_wmm_param(vap, WLAN_WME_CWMAX,
                        bss, ac, value);
            }
            break;
        case IEEE80211_WMMPARAMS_AIFS:
            if (value < 0 ||  value > 15) {
                retv = -EINVAL;
            }
            else {
                retv = wlan_set_wmm_param(vap, WLAN_WME_AIFS,
                        bss, ac, value);
            }
            break;
        case IEEE80211_WMMPARAMS_TXOPLIMIT:
            if (value < 0 ||  value > (8192 >> 5)) {
                retv = -EINVAL;
            }
            else {
                retv = wlan_set_wmm_param(vap, WLAN_WME_TXOPLIMIT,
                        bss, ac, value);
            }
            break;
        case IEEE80211_WMMPARAMS_ACM:
            if (value < 0 ||  value > 1) {
                retv = -EINVAL;
            }
            else {
                retv = wlan_set_wmm_param(vap, WLAN_WME_ACM,
                        bss, ac, value);
            }
            break;
        case IEEE80211_WMMPARAMS_NOACKPOLICY:
            if (value < 0 ||  value > 1) {
                retv = -EINVAL;
            }
            else {
                retv = wlan_set_wmm_param(vap, WLAN_WME_ACKPOLICY,
                        bss, ac, value);
            }
            break;
#if UMAC_VOW_DEBUG
        case IEEE80211_PARAM_VOW_DBG_CFG:
            {
                if(ac >= MAX_VOW_CLIENTS_DBG_MONITOR ) {
                    qdf_print("Invalid Parameter: Acceptable index range [0 - %d]",
                            MAX_VOW_CLIENTS_DBG_MONITOR-1);
                    retv = -EINVAL;
                } else {
                    osifp->tx_dbg_vow_peer[ac][0] = bss;
                    osifp->tx_dbg_vow_peer[ac][1] = value;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                    if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                        ic->nss_vops->ic_osif_nss_vdev_vow_dbg_cfg(osifp, ac);
                    }
#endif
                }
            }
            break;
#endif

        default:
            return retv;
    }
    /* Reinitialise the vaps to update the wme params during runtime configuration  */
    if (!bss && retv == EOK) {
        retv = IS_UP(osifp->netdev) ? osif_vap_init(osifp->netdev,0) : 0;
    }
    /*
     * As we are not doing VAP reinit in case of (bss & retv == EOK),
     * we need to update beacon template
     */
    if (bss && retv == EOK) {
        wlan_vdev_beacon_update(vap);
    }
    return retv;
}


int ieee80211_ucfg_getwmmparams(void *osif, int wmmparam, int ac, int bss)
{
    int value = 0;
    osif_dev *osifp = (osif_dev *)osif;
    wlan_if_t vap = osifp->os_if;

    switch (wmmparam)
    {
        case IEEE80211_WMMPARAMS_CWMIN:
            value = wlan_get_wmm_param(vap, WLAN_WME_CWMIN,
                    bss, ac);
            break;
        case IEEE80211_WMMPARAMS_CWMAX:
            value = wlan_get_wmm_param(vap, WLAN_WME_CWMAX,
                    bss, ac);
            break;
        case IEEE80211_WMMPARAMS_AIFS:
            value = wlan_get_wmm_param(vap, WLAN_WME_AIFS,
                    bss, ac);
            break;
        case IEEE80211_WMMPARAMS_TXOPLIMIT:
            value = wlan_get_wmm_param(vap, WLAN_WME_TXOPLIMIT,
                    bss, ac);
            break;
        case IEEE80211_WMMPARAMS_ACM:
            value = wlan_get_wmm_param(vap, WLAN_WME_ACM,
                    bss, ac);
            break;
        case IEEE80211_WMMPARAMS_NOACKPOLICY:
            value = wlan_get_wmm_param(vap, WLAN_WME_ACKPOLICY,
                    bss, ac);
            break;
        default:
            value = -EINVAL;
            break;
    }

    return value;

}

int ieee80211_ucfg_set_muedcaparams(void *osif, uint8_t muedcaparam,
        uint8_t ac, uint8_t value)
{

    int ret = 0;
    osif_dev *osifp = (osif_dev *)osif;
    wlan_if_t vap = osifp->os_if;

    /* Check for the lower and upper bounds for paramID */
    if((muedcaparam < IEEE80211_MUEDCAPARAMS_ECWMIN) &&
            (muedcaparam > IEEE80211_MUEDCAPARAMS_TIMER)) {
        qdf_print("%s: paramID should be between %d-%d.",
                __func__, IEEE80211_MUEDCAPARAMS_ECWMIN,
                IEEE80211_MUEDCAPARAMS_TIMER);
        ret = -EINVAL;
    }

    /* Check for the lower and upper bounds for AC */
    if(ac >= MUEDCA_NUM_AC) {
        qdf_print("%s: AC should be less than %d.",
                __func__, MUEDCA_NUM_AC);
        ret = -EINVAL;
    }

    switch (muedcaparam)
    {

        case IEEE80211_MUEDCAPARAMS_ECWMIN:
            if(value > MUEDCA_ECW_MAX) {
                qdf_print("%s: ECWMIN should be less than %d.",
                        __func__, MUEDCA_ECW_MAX);
                ret =  -EINVAL;
            }
            else {
                ret = wlan_set_muedca_param(vap, WLAN_MUEDCA_ECWMIN,
                        ac, value);
            }
            break;

        case IEEE80211_MUEDCAPARAMS_ECWMAX:
            if(value > MUEDCA_ECW_MAX) {
                qdf_print("%s: ECWMAX should be less than equal %d.",
                        __func__, MUEDCA_ECW_MAX);
                ret =  -EINVAL;
            }
            else {
                ret = wlan_set_muedca_param(vap, WLAN_MUEDCA_ECWMAX,
                        ac, value);
            }
            break;

        case IEEE80211_MUEDCAPARAMS_AIFSN:
            if(value > MUEDCA_AIFSN_MAX) {
                qdf_print("%s: AIFSN should less than equal to %d.",
                        __func__, MUEDCA_AIFSN_MAX);
                ret =  -EINVAL;
            }
            else {
                ret = wlan_set_muedca_param(vap, WLAN_MUEDCA_AIFSN,
                        ac, value);
            }
            break;

        /* ACM value set to 1 signifies admission control is enabled,
         * while a value of 0 signifies admission control is disabled for
         * the corresponding AC. */
        case IEEE80211_MUEDCAPARAMS_ACM:
            if(value > 1) {
                qdf_print("%s:ACM value should be 0 or 1.", __func__);
                ret =  -EINVAL;
            }
            else {
                ret = wlan_set_muedca_param(vap, WLAN_MUEDCA_ACM,
                        ac, value);
            }
            break;

        case IEEE80211_MUEDCAPARAMS_TIMER:
            if(value > MUEDCA_TIMER_MAX) {
                qdf_print("%s:Timer value should be less than equal to %d.",
                        __func__, MUEDCA_TIMER_MAX);
                ret =  -EINVAL;
            }
            else {
                ret = wlan_set_muedca_param(vap, WLAN_MUEDCA_TIMER,
                        ac, value);
            }
            break;

        default:
            ret = -EINVAL;
            break;

    }

    if(vap->iv_he_muedca == IEEE80211_MUEDCA_STATE_ENABLE) {
        wlan_vdev_beacon_update(vap);
    }

    return ret;
}

int ieee80211_ucfg_get_muedcaparams(void *osif, uint8_t muedcaparam, uint8_t ac)
{
    int value = 0;
    osif_dev *osifp = (osif_dev *)osif;
    wlan_if_t vap = osifp->os_if;

    /* Check for the lower and upper bounds for paramID */
    if((muedcaparam < IEEE80211_MUEDCAPARAMS_ECWMIN) &&
            (muedcaparam > IEEE80211_MUEDCAPARAMS_TIMER)) {
        qdf_print("%s: paramID should be between %d-%d.",
                __func__, IEEE80211_MUEDCAPARAMS_ECWMIN,
                IEEE80211_MUEDCAPARAMS_TIMER);
        value = -EINVAL;
    }

    /* Check for the lower and upper bounds for AC */
    if(ac >= MUEDCA_NUM_AC) {
        qdf_print("%s: AC should be less than %d.",
                __func__, MUEDCA_NUM_AC);
        value = -EINVAL;
    }

    switch (muedcaparam)
    {

        case IEEE80211_MUEDCAPARAMS_ECWMIN:
            value = wlan_get_muedca_param(vap, WLAN_MUEDCA_ECWMIN, ac);
            break;

        case IEEE80211_MUEDCAPARAMS_ECWMAX:
            value = wlan_get_muedca_param(vap, WLAN_MUEDCA_ECWMAX, ac);
            break;

        case IEEE80211_MUEDCAPARAMS_AIFSN:
            value = wlan_get_muedca_param(vap, WLAN_MUEDCA_AIFSN, ac);
            break;

        case IEEE80211_MUEDCAPARAMS_ACM:
            value = wlan_get_muedca_param(vap, WLAN_MUEDCA_ACM, ac);
            break;

        case IEEE80211_MUEDCAPARAMS_TIMER:
            value = wlan_get_muedca_param(vap, WLAN_MUEDCA_TIMER, ac);
            break;

        default:
            value = -EINVAL;
            break;
    }

    return value;

}

static void
domlme(void *arg, wlan_node_t node)
{
    struct mlmeop *op = (struct mlmeop *)arg;

    switch (op->mlme->im_op) {

        case IEEE80211_MLME_DISASSOC:
            ieee80211_try_mark_node_for_delayed_cleanup(node);
            wlan_mlme_disassoc_request(op->vap,wlan_node_getmacaddr(node),op->mlme->im_reason);
            break;
        case IEEE80211_MLME_DEAUTH:
            IEEE80211_DPRINTF(op->vap, IEEE80211_MSG_AUTH, "%s: sending DEAUTH to %s, domlme deauth reason %d\n",
                    __func__, ether_sprintf(wlan_node_getmacaddr(node)), op->mlme->im_reason);
            ieee80211_try_mark_node_for_delayed_cleanup(node);
            wlan_mlme_deauth_request(op->vap,wlan_node_getmacaddr(node),op->mlme->im_reason);
            break;
        case IEEE80211_MLME_ASSOC:
            wlan_mlme_assoc_resp(op->vap,wlan_node_getmacaddr(node),op->mlme->im_reason, 0, NULL);
            break;
        case IEEE80211_MLME_REASSOC:
            wlan_mlme_assoc_resp(op->vap,wlan_node_getmacaddr(node),op->mlme->im_reason, 1, NULL);
            break;
        default:
            break;
    }
}

int
ieee80211_ucfg_setmlme(struct ieee80211com *ic, void *osif, struct ieee80211req_mlme *mlme)
{
    struct ieee80211_app_ie_t optie;
    osif_dev *osifp = (osif_dev *)osif;
    wlan_if_t vap = osifp->os_if;
    struct ieee80211vap *tempvap = NULL;
    struct ieee80211_node *ni = NULL;

    if ((vap == NULL) || (ic == NULL) ) {
        return -EINVAL;
    }

    optie.ie = &mlme->im_optie[0];
    optie.length = mlme->im_optie_len;

    switch (mlme->im_op) {
        case IEEE80211_MLME_ASSOC:
            /* set dessired bssid when in STA mode accordingly */
            if (wlan_vap_get_opmode(vap) != IEEE80211_M_STA &&
                    osifp->os_opmode != IEEE80211_M_P2P_DEVICE &&
                    osifp->os_opmode != IEEE80211_M_P2P_CLIENT) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                        "[%s] non sta mode, skip to set bssid\n", __func__);
            } else {
                u_int8_t des_bssid[IEEE80211_ADDR_LEN];

                if (!IS_NULL_ADDR(mlme->im_macaddr)) {
                    /*If AP mac to which our sta vap is trying to connect has
                    same mac as one of our ap vaps ,dont set that as sta bssid */
                    TAILQ_FOREACH(tempvap, &ic->ic_vaps, iv_next) {
                        if (tempvap->iv_opmode == IEEE80211_M_HOSTAP && IEEE80211_ADDR_EQ(tempvap->iv_myaddr,mlme->im_macaddr)) {
                                qdf_print("[%s] Mac collision for [%s]",__func__,ether_sprintf(mlme->im_macaddr));
                                return -EINVAL;
                        }
                    }
                    IEEE80211_ADDR_COPY(des_bssid, &mlme->im_macaddr[0]);
                    wlan_aplist_set_desired_bssidlist(vap, 1, &des_bssid);
                    qdf_print("[%s] set desired bssid %02x:%02x:%02x:%02x:%02x:%02x",__func__,des_bssid[0],
                            des_bssid[1],des_bssid[2],des_bssid[3],des_bssid[4],des_bssid[5]);
                }
            }

            if (osifp->os_opmode == IEEE80211_M_STA ||
                    (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_GO ||
                    (u_int8_t)osifp->os_opmode == IEEE80211_M_P2P_CLIENT ||
                    osifp->os_opmode == IEEE80211_M_IBSS) {
                vap->iv_mlmeconnect=1;
#if UMAC_SUPPORT_WPA3_STA
                if (wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE)
                                          & (uint32_t)((1 << WLAN_CRYPTO_AUTH_SAE))) {
                   vap->iv_sta_external_auth_enabled = true;
                   osifp->app_filter |= IEEE80211_FILTER_TYPE_AUTH;
                } else {
                   vap->iv_sta_external_auth_enabled = false;
                }
#endif
                osif_vap_init(osifp->netdev, 0);
            }
            else if (osifp->os_opmode ==  IEEE80211_M_HOSTAP) {
                /* NB: the broadcast address means do 'em all */
                if (!IEEE80211_ADDR_EQ(mlme->im_macaddr, ieee80211broadcastaddr)) {
                ni = ieee80211_vap_find_node(vap, mlme->im_macaddr);
                if (ni == NULL)
                    return -EINVAL;
                else
                    ic = ni->ni_ic;
                if (ieee80211_is_pmf_enabled(vap, ni)
                        && vap->iv_opmode == IEEE80211_M_HOSTAP
                        && (vap->iv_pmf_enable > 0)
                        && (ni->ni_flags & IEEE80211_NODE_AUTH)) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                        "[%s] drop assoc resp for pmf client from hostapd\n",
                        __func__);
                } else {
                    wlan_mlme_assoc_resp(vap,mlme->im_macaddr,mlme->im_reason, 0, &optie);
                }
                ieee80211_free_node(ni);
                } else {
                    struct mlmeop iter_arg;
                    iter_arg.mlme = mlme;
                    iter_arg.vap = vap;
                    wlan_iterate_station_list(vap,domlme,&iter_arg);
                }
            }
            else
                return -EINVAL;
            break;
        case IEEE80211_MLME_REASSOC:
            if (osifp->os_opmode == IEEE80211_M_STA ||
                    osifp->os_opmode == IEEE80211_M_P2P_GO ||
                    osifp->os_opmode == IEEE80211_M_P2P_CLIENT) {
                osif_vap_init(osifp->netdev, 0);
            }
            else if (osifp->os_opmode ==  IEEE80211_M_HOSTAP) {
                /* NB: the broadcast address means do 'em all */
                if (!IEEE80211_ADDR_EQ(mlme->im_macaddr, ieee80211broadcastaddr)) {
                ni = ieee80211_vap_find_node(vap, mlme->im_macaddr);
                if (ni == NULL)
                    return -EINVAL;
                else
                    ic = ni->ni_ic;
                if(ieee80211_is_pmf_enabled(vap, ni) &&
                        vap->iv_opmode == IEEE80211_M_HOSTAP &&
                        (vap->iv_pmf_enable > 0) &&
                        (ni->ni_flags & IEEE80211_NODE_AUTH))
                {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                        "[%s] drop assoc resp for pmf client from hostapd\n",
                        __func__);
                    ieee80211_free_node(ni);
                    return 1; /* drop assoc resp for pmf client from hostapd */
                }
                    wlan_mlme_assoc_resp(vap,mlme->im_macaddr,mlme->im_reason, 1, &optie);
                    ieee80211_free_node(ni);
                } else {
                    struct mlmeop iter_arg;
                    iter_arg.mlme = mlme;
                    iter_arg.vap = vap;
                    wlan_iterate_station_list(vap,domlme,&iter_arg);
                }
            }
            else
                return -EINVAL;
            break;
        case IEEE80211_MLME_AUTH_FILS:
        case IEEE80211_MLME_AUTH:
            if (osifp->os_opmode != IEEE80211_M_HOSTAP) {
                return -EINVAL;
            }
            /* NB: ignore the broadcast address */
            if (!IEEE80211_ADDR_EQ(mlme->im_macaddr, ieee80211broadcastaddr)) {
#if WLAN_SUPPORT_FILS
                if(mlme->im_op == IEEE80211_MLME_AUTH_FILS && wlan_fils_is_enable(vap->vdev_obj)) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_FILS,
                                "%s op : 0x%x ie.len : %d mac %s\n",
                                __func__, mlme->im_op, optie.length,
                                ether_sprintf(mlme->im_macaddr));
                    if (wlan_mlme_auth_fils(vap->vdev_obj, &mlme->fils_aad, mlme->im_macaddr)) {
                        qdf_print("%s: FILS crypto registration failed", __func__);
                    } else {
                        qdf_print("%s: FILS crypto registered successfully", __func__);
                    }
                }
#endif
                wlan_mlme_auth(vap,mlme->im_macaddr,mlme->im_seq,mlme->im_reason, NULL, 0, &optie);
            }
            break;
        case IEEE80211_MLME_DISASSOC:
        case IEEE80211_MLME_DEAUTH:
            switch (osifp->os_opmode) {
                case IEEE80211_M_STA:
                case IEEE80211_M_P2P_CLIENT:
                    //    if (mlme->im_op == IEEE80211_MLME_DISASSOC && !osifp->is_p2p_interface) {
                    //        return -EINVAL; /*fixme darwin does this, but linux did not before? */
                    //    }

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
                    if(wlan_mlme_is_stacac_running(vap)) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "Do not stop the BSS STA CAC is on\n");
                        return -EINVAL;
                    } else {
                        osif_vap_stop(osifp->netdev);
                    }
#else
                    osif_vap_stop(osifp->netdev);
#endif
                    break;
                case IEEE80211_M_HOSTAP:
                case IEEE80211_M_IBSS:
                    if ( ic && ic->ic_is_mode_offload(ic)){
                        /*No need to put  any check for Broadcast as for braodcast ni == NULL */
                        if (IEEE80211_ADDR_EQ(mlme->im_macaddr, vap->iv_myaddr)) {
                            qdf_print("Cannot send Disassoc to self ");
                            return -EINVAL;
                        }
                        ni = ieee80211_vap_find_node(vap, mlme->im_macaddr);
                        if (ni) {
                            /* If DA is non_bss unicast address, mark this node
                             * delayed node cleanup candidate.
                             */
                            ieee80211_try_mark_node_for_delayed_cleanup(ni);
                            /* claim node immediately */
                            ieee80211_free_node(ni);
                        }
                    }
                    /* the 'break' statement for this case is intentionally removed, to make sure that this fall through next statement */
                case IEEE80211_M_P2P_GO:
                    /* NB: the broadcast address means do 'em all */
                    if (!IEEE80211_ADDR_EQ(mlme->im_macaddr, ieee80211broadcastaddr)) {
                        if (mlme->im_op == IEEE80211_MLME_DEAUTH) {
                            wlan_mlme_deauth_request(vap,mlme->im_macaddr,mlme->im_reason);
                        }
                        if (mlme->im_op == IEEE80211_MLME_DISASSOC) {
                            wlan_mlme_disassoc_request(vap,mlme->im_macaddr,mlme->im_reason);
                        }
                    } else {
                        if (wlan_vap_is_pmf_enabled(vap)) {
                            if (mlme->im_op == IEEE80211_MLME_DEAUTH) {
                                IEEE80211_DPRINTF(vap, IEEE80211_MSG_AUTH, "%s: sending DEAUTH to %s, mlme deauth reason %d\n",
                                        __func__, ether_sprintf(mlme->im_macaddr), mlme->im_reason);
                                wlan_mlme_deauth_request(vap,mlme->im_macaddr,mlme->im_reason);
                            }
                            if (mlme->im_op == IEEE80211_MLME_DISASSOC) {
                                wlan_mlme_disassoc_request(vap,mlme->im_macaddr,mlme->im_reason);
                            }
                        } else {
                            struct mlmeop iter_arg;
                            iter_arg.mlme = mlme;
                            iter_arg.vap = vap;
                            wlan_iterate_station_list(vap,domlme,&iter_arg);
                        }
                    }
                    break;
                default:
                    return -EINVAL;
            }
            break;
        case IEEE80211_MLME_AUTHORIZE:
        case IEEE80211_MLME_UNAUTHORIZE:
            if (osifp->os_opmode != IEEE80211_M_HOSTAP &&
                    osifp->os_opmode != IEEE80211_M_STA &&
                    osifp->os_opmode != IEEE80211_M_P2P_GO) {
                return -EINVAL;
            }
            if (mlme->im_op == IEEE80211_MLME_AUTHORIZE) {
                wlan_node_authorize(vap, 1, mlme->im_macaddr);
            } else {
                wlan_node_authorize(vap, 0, mlme->im_macaddr);
            }
            break;
        case IEEE80211_MLME_CLEAR_STATS:
#ifdef notyet
            struct cdp_peer *peer_dp_handle;

            if (vap->iv_opmode != IEEE80211_M_HOSTAP)
                return -EINVAL;
            ni = ieee80211_find_node(&ic->ic_sta, mlme->im_macaddr);
            if (ni == NULL)
                return -ENOENT;

            peer_dp_handle = wlan_peer_get_dp_handle(node->peer_obj);
            if (!peer_dp_handle)
                return -ENOENT;

            /* clear statistics */
            cdp_host_reset_peer_stats(wlan_pdev_get_psoc(ic->ic_pdev_obj),
                                      peer_dp_handle);
            ieee80211_free_node(ni);
#endif
            break;

        case IEEE80211_MLME_STOP_BSS:
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            if(wlan_mlme_is_stacac_running(vap)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "Do not stop the BSS STA CAC is on\n");
                return -EINVAL;
            } else {
                osif_vap_stop(osifp->netdev);
            }
#else
            osif_vap_stop(osifp->netdev);
#endif
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

#if QCA_SUPPORT_GPR
int ieee80211_ucfg_send_gprparams(wlan_if_t vap, uint8_t command)
{
    struct ieee80211com *ic = vap->iv_ic;
    int retv = -EINVAL;
    acfg_netlink_pvt_t *acfg_nl;

    if ( !ic || !ic->ic_acfg_handle ) {
        return -EINVAL;
    }
    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;

    /* Give access to only one app on one radio */
    if(qdf_semaphore_acquire_intr(acfg_nl->sem_lock)){
        /*failed to acquire mutex*/
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                 "%s(): failed to acquire mutex!\n", __func__);
        return -EAGAIN;
    }

    switch (command)
    {
        case IEEE80211_GPR_DISABLE:
            if (ic->ic_gpr_enable) {
                if (vap->iv_opmode == IEEE80211_M_HOSTAP ) {
                    if (vap->iv_gpr_enable == 1) {
                        vap->iv_gpr_enable = 0;
                        ic->ic_gpr_enable_count--;
                    } else {
                        qdf_err("GPR already stopped on this vap or not started yet !\n");
                    }
                    if (ic->ic_gpr_enable_count == 0) {
                        qdf_hrtimer_kill(&ic->ic_gpr_timer);
                        qdf_mem_free(ic->acfg_frame);
                        ic->acfg_frame = NULL;
                        ic->ic_gpr_enable = 0;
                        qdf_err("\nStopping GPR timer as this is last vap with gpr \n");
                    }
                    retv = EOK;
                } else {
                    qdf_err("Invalid! Allowed only in HOSTAP mode\n");
                }
            } else {
                qdf_err("GPR not started on any of vap, start GPR first \n");
            }
            break;
        case IEEE80211_GPR_ENABLE:
            if (ic->ic_gpr_enable) {
                if (vap->iv_opmode == IEEE80211_M_HOSTAP ) {
                    if (vap->iv_gpr_enable == 0) {
                        vap->iv_gpr_enable = 1;
                        ic->ic_gpr_enable_count++;
                        retv = EOK;
                    } else {
                        qdf_err("GPR already started on this vap \n");
                    }
                } else {
                    qdf_err("Invalid! Allowed only in HOSTAP mode\n");
                }
            } else {
                qdf_err("GPR not started on any of vap, start GPR first \n");
            }
            break;
        case IEEE80211_GPR_PRINT_STATS:
            if (ic->ic_gpr_enable) {
                qdf_hrtimer_data_t *gpr_hrtimer = &ic->ic_gpr_timer;
                qdf_err("Timer STATS for GPR \n");
                if (vap->iv_opmode == IEEE80211_M_HOSTAP ) {
                    qdf_err("------------------------------------\n");
                    qdf_err("| GPR on this vap enabled    - %d   |\n", vap->iv_gpr_enable);
                    qdf_err("| GPR on radio enabled       - %d   |\n", ic->ic_gpr_enable);
                    qdf_err("| GPR state                  - %s   |\n", (qdf_hrtimer_active(&ic->ic_gpr_timer))?"Active":"Dormant");
                    qdf_err("| GPR Minimum Period         - %d   |\n", DEFAULT_MIN_GRATITOUS_PROBE_RESP_PERIOD);
                    qdf_err("| Current Timestamp          - %lld |\n", ktime_to_ns(qdf_ktime_get()));
                    qdf_err("| Timer expires in           - %lld |\n", ktime_to_ns(ktime_add(qdf_ktime_get(),
                                                          qdf_hrtimer_get_remaining(gpr_hrtimer))));
                    qdf_err("| GPR timer start count      - %u   |\n", ic->ic_gpr_timer_start_count);
                    qdf_err("| GPR timer resize count     - %u   |\n", ic->ic_gpr_timer_resize_count);
                    qdf_err("| GPR timer send count       - %u   |\n", ic->ic_gpr_send_count);
                    qdf_err("| GPR timer user period      - %u   |\n", ic->ic_period);
                    qdf_err("| GPR enabled vap count      - %u   |\n", ic->ic_gpr_enable_count);
                    qdf_err("------------------------------------\n");
                    retv = EOK;
                } else {
                    qdf_err("Invalid! Allowed only in HOSTAP mode\n");
                }
            } else {
                qdf_err("GPR not running on any of the Vaps\n");
            }
            break;
        case IEEE80211_GPR_CLEAR_STATS:
            if (ic->ic_gpr_enable) {
                qdf_err("%s %d Timer CLEAR STATS for GPR \n",__func__,__LINE__);
                ic->ic_gpr_timer_start_count  = 0;
                ic->ic_gpr_timer_resize_count = 0;
                ic->ic_gpr_send_count         = 0;
                retv = EOK;
            } else {
                qdf_err("GPR not running on any of the Vaps\n");
            }
            break;
        default:
            qdf_err("Invalid command type\n");
    }
    qdf_semaphore_release(acfg_nl->sem_lock);
    return retv;
}
#endif
