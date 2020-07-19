/*
 * Copyright (c) 2016-2017,2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary
 */


#include <osif_private.h>
#include <ieee80211_var.h>
#include <ieee80211_ioctl.h>
#include <ieee80211_api.h>
#include <ieee80211.h>
#include <ol_if_athvar.h>
#include <ext_ioctl_drv_if.h>
#include <ieee80211_acs_internal.h>
#include <wlan_reg_services_api.h>

/* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on channel switching rules
 */

/**
 * @brief find new channel based on request and trigger channel change
 *
 * @param    ic : pointer to struct ieee80211com
 * @param    request : pointer to wifi_channel_switch_request_t
 *
 * @return   0 if channel change is triggered successfully else negative
 *           error code is returned.
 */

static int
ieee80211_trigger_channel_switch (struct ieee80211com *ic,
        wifi_channel_switch_request_t *request)
{

    struct ieee80211_ath_channel *ch        = NULL;
    struct ieee80211_ath_channel *tgt_ch    = NULL;
    int error                           = 0;
    u_int64_t flags                     = 0;
    int freq                            = 0;
    u_int16_t channel_width[WIFI_OPERATING_CHANWIDTH_MAX_ELEM] =
    {CHWIDTH_20, CHWIDTH_40, CHWIDTH_80, CHWIDTH_160, CHWIDTH_160};

    if (!ic || !request) {
        return -EINVAL;
    }

    ch = ic->ic_curchan;

    if ((IEEE80211_IS_CHAN_A(ch) || IEEE80211_IS_CHAN_B(ch) ||
         IEEE80211_IS_CHAN_G(ch) || IEEE80211_IS_CHAN_PUREG(ch) ||
         IEEE80211_IS_CHAN_ANYG(ch))) {
            flags = ch->ic_flags;
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Dest chan is A/B/G/PUREG/ANYG\n", __func__);
    } else if (IEEE80211_IS_CHAN_11NG(ch)) {
        if (request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_40MHZ) {
            if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_PLUS) {
                flags = IEEE80211_CHAN_11NG_HT40PLUS;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11NG_HT40PLUS\n", __func__);
            } else if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_MINUS) {
                flags = IEEE80211_CHAN_11NG_HT40MINUS;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11NG_HT40MINUS\n", __func__);
            } else {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Invalid secondary offset %d specified with 40Mhz width\n",
                        __func__, request->sec_chan_offset);
                error = -EINVAL;
            }
        } else if (request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_20MHZ) {
            flags = IEEE80211_CHAN_11NG_HT20;
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                    "%s: Dest chan is IEEE80211_CHAN_11NG_HT20\n", __func__);
        } else {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                    "%s: Invalid chan width %d requested for 11N mode\n",
                    __func__, request->target_chanwidth);
            error = -EINVAL;
        }
    } else if (IEEE80211_IS_CHAN_11NA(ch)) {
        if (request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_40MHZ) {
            if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_PLUS) {
                flags = IEEE80211_CHAN_11NA_HT40PLUS;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11NA_HT40PLUS\n\n", __func__);
            } else if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_MINUS) {
                flags = IEEE80211_CHAN_11NA_HT40MINUS;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11NA_HT40MINUS\n\n", __func__);
            } else {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Invalid secondary offset %d specified with 40Mhz width\n",
                        __func__, request->sec_chan_offset);
                error = -EINVAL;
            }
        } else if (request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_20MHZ) {
            flags = IEEE80211_CHAN_11NA_HT20;
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                    "%s: Dest chan is IEEE80211_CHAN_11NA_HT20\n\n", __func__);
        } else {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                    "%s: Invalid chan width %d requested for 11N mode\n",
                    __func__, request->target_chanwidth);
            error = -EINVAL;
        }
    } else if (IEEE80211_IS_CHAN_VHT(ch)) {
        switch (request->target_chanwidth) {
            case WIFI_OPERATING_CHANWIDTH_20MHZ:
                flags = IEEE80211_CHAN_11AC_VHT20;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AC_VHT20\n\n", __func__);
                break;
            case WIFI_OPERATING_CHANWIDTH_40MHZ:
                if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_PLUS) {
                    flags = IEEE80211_CHAN_11AC_VHT40PLUS;
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                            "%s: Dest chan is IEEE80211_CHAN_11AC_VHT40PLUS\n\n", __func__);
                } else if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_MINUS) {
                    flags = IEEE80211_CHAN_11AC_VHT40MINUS;
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                            "%s: Dest chan is IEEE80211_CHAN_11AC_VHT40MINUS\n\n", __func__);
                } else {
                    /* should not land here */
                    printk(" Invalid secondary chan offset received: %d\n",
                            request->sec_chan_offset);
                    error = -EINVAL;
                }
                break;
            case WIFI_OPERATING_CHANWIDTH_80MHZ:
                flags = IEEE80211_CHAN_11AC_VHT80;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AC_VHT80\n\n", __func__);
                break;
            case WIFI_OPERATING_CHANWIDTH_160MHZ:
                flags = IEEE80211_CHAN_11AC_VHT160;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AC_VHT160\n\n", __func__);
                break;
            case WIFI_OPERATING_CHANWIDTH_80_80MHZ:
                flags = IEEE80211_CHAN_11AC_VHT80_80;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AC_VHT80_80\n\n", __func__);
                break;
            default:
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Error: invalid chan width %d specified\n", __func__,
                        request->target_chanwidth);
                error = -EINVAL;
        }
    } else if (IEEE80211_IS_CHAN_11AXG(ch)) {
        if (request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_40MHZ) {
            if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_PLUS) {
                flags = IEEE80211_CHAN_11AXG_HE40PLUS;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AXG_HE40PLUS\n", __func__);
            } else if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_MINUS) {
                flags = IEEE80211_CHAN_11AXG_HE40MINUS;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AXG_HE40MINUS\n", __func__);
            } else {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Invalid secondary offset %d specified for 11AXG 40 MHz\n",
                        __func__, request->sec_chan_offset);
                error = -EINVAL;
            }
        } else if (request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_20MHZ) {
            flags = IEEE80211_CHAN_11AXG_HE20;
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                    "%s: Dest chan is IEEE80211_CHAN_11AXG_HE20\n", __func__);
        } else {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                    "%s: Invalid chan width %d requested for 11AXG\n",
                    __func__, request->target_chanwidth);
            error = -EINVAL;
        }
    } else if (IEEE80211_IS_CHAN_11AXA(ch)) {
        switch (request->target_chanwidth) {
            case WIFI_OPERATING_CHANWIDTH_20MHZ:
                flags = IEEE80211_CHAN_11AXA_HE20;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AXA_HE20\n\n", __func__);
                break;
            case WIFI_OPERATING_CHANWIDTH_40MHZ:
                if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_PLUS) {
                    flags = IEEE80211_CHAN_11AXA_HE40PLUS;
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                            "%s: Dest chan is IEEE80211_CHAN_11AXA_HE40PLUS\n\n", __func__);
                } else if (request->sec_chan_offset == WIFI_SEC_CHAN_OFFSET_IS_MINUS) {
                    flags = IEEE80211_CHAN_11AXA_HE40MINUS;
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                            "%s: Dest chan is IEEE80211_CHAN_11AXA_HE40MINUS\n\n", __func__);
                } else {
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                            "%s: Invalid secondary offset %d specified for 11AXA 40 MHz\n",
                            __func__, request->sec_chan_offset);
                    error = -EINVAL;
                }
                break;
            case WIFI_OPERATING_CHANWIDTH_80MHZ:
                flags = IEEE80211_CHAN_11AXA_HE80;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AXA_HE80\n\n", __func__);
                break;
            case WIFI_OPERATING_CHANWIDTH_160MHZ:
                flags = IEEE80211_CHAN_11AXA_HE160;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AXA_HE160\n\n", __func__);
                break;
            case WIFI_OPERATING_CHANWIDTH_80_80MHZ:
                flags = IEEE80211_CHAN_11AXA_HE80_80;
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Dest chan is IEEE80211_CHAN_11AXA_HE80_80\n\n", __func__);
                break;
            default:
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                        "%s: Invalid chan width %d requested for 11AXA\n",
                        __func__, request->target_chanwidth);
                error = -EINVAL;
        }
    } else {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: unknown operating mode\n", __func__);
        error = -EINVAL;
    }


    if (error) {
        return error;
    }

    freq = ieee80211_ieee2mhz(ic, request->target_pchannel, 0);
    tgt_ch = ieee80211_find_channel(ic, freq, request->target_cfreq2, flags);

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
            "%s: Destination chan: %d(%dMhz):0x%pK, flag: 0x%x, cfreq2: %d, \n",
            __func__, request->target_pchannel, freq, tgt_ch, flags, request->target_cfreq2);


    if (!tgt_ch) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: couldnt find requested chanel: %d\n", __func__,
                request->target_pchannel);
        return -EINVAL;
    }

    /* if current channel and BW is same as requested, we are done. */
    if (ch == tgt_ch) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Requested chanel is same as current operating channel\n", __func__);
        /* Set same_chan flag for repeater move case */
        ic->ic_repeater_move.same_chan = true;
        return EOK;
    }

    ic->ic_chanchange_chwidth = channel_width[request->target_chanwidth];
    ic->ic_chanchange_channel = tgt_ch;
    ic->ic_chanchange_secoffset = ieee80211_sec_chan_offset(tgt_ch);
    ic->ic_chanchange_chanflag = flags;
    ic->ic_chanchange_chan = request->target_pchannel;
    ic->ic_repeater_move.same_chan = false;
    /* Use user supplied value of num_csa if its present.
     * Otherwise use default count.
     */
    if (request->num_csa) {
        ic->ic_chanchange_tbtt = request->num_csa;
    } else {
        ic->ic_chanchange_tbtt = IEEE80211_DEFAULT_CHANSWITCH_COUNT;
    }

    wlan_pdev_mlme_vdev_sm_csa_restart(ic->ic_pdev_obj, tgt_ch);

    ic->ic_flags_ext2 |= IEEE80211_FEXT2_CSA_WAIT;
    ieee80211com_set_flags(ic, IEEE80211_F_CHANSWITCH);


    return EOK;
}

static int
ieee80211_check_chan_switch_sanity (struct ieee80211com *ic,
                                    struct ieee80211_ath_channel *ch,
                                    wifi_channel_switch_request_t *request)
{
    /* verifying each input parameter for correctness may become costly.
     * only basic sanity is performed here.
     */

    if ((request->target_pchannel > IEEE80211_CHANNEL_MAX) ||
            (request->target_cfreq2 > IEEE80211_CHANNEL_MAX)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Invalid channel specified: primary: %d, cfreq2: %d\n",
                __func__, request->target_pchannel, request->target_cfreq2);
        return -EINVAL;
    }

    if ((request->target_chanwidth < WIFI_OPERATING_CHANWIDTH_20MHZ) || (request->target_chanwidth > WIFI_OPERATING_CHANWIDTH_80_80MHZ)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: invalid chan width requested %d\n", __func__, request->target_chanwidth);
        return -EINVAL;
    }

    if ((request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_40MHZ) &&
            ((request->sec_chan_offset != WIFI_SEC_CHAN_OFFSET_IS_PLUS) &&
             (request->sec_chan_offset != WIFI_SEC_CHAN_OFFSET_IS_MINUS))) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: invalid secondary chanel offset %d for 40MhZ\n",
                __func__, request->sec_chan_offset);
        return -EINVAL;
    }

    if ((request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_80_80MHZ) && (request->target_cfreq2 < IEEE80211_CHANNEL_MIN_5G)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: invalid target_cfreq2: %d\n", __func__, request->target_cfreq2);
        return -EINVAL;
    }

    if ((request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_160MHZ) &&
            !IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT160) &&
            !IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE160)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: 160 MHz is not supported by this PHY\n", __func__);
        return -EINVAL;
    }

    if ((request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_80_80MHZ) &&
            !IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT80_80) &&
            !IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE80_80)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: 80+80 MHz is not supported by this PHY\n", __func__);
        return -EINVAL;
    }

    if (request->target_cfreq2 &&
            (request->target_chanwidth != WIFI_OPERATING_CHANWIDTH_80_80MHZ)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: cfreq2 is applicable only for 80+80 MHz\n", __func__);
        return -EINVAL;
    }

    /* Ensure requested chan width is allowed in current mode (A/B/G/N/AC) */
    if ((IEEE80211_IS_CHAN_A(ch) || IEEE80211_IS_CHAN_B(ch) ||
                IEEE80211_IS_CHAN_G(ch) || IEEE80211_IS_CHAN_PUREG(ch) ||
                IEEE80211_IS_CHAN_ANYG(ch)) && (request->target_chanwidth > WIFI_OPERATING_CHANWIDTH_20MHZ)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: chan width %d requested for A/B/G mode:\n",
                __func__, request->target_chanwidth);
        return -EINVAL;
    }

    if (IEEE80211_IS_CHAN_11N(ch) && (request->target_chanwidth > WIFI_OPERATING_CHANWIDTH_40MHZ)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: chan width %d requested for 11N mode:\n",
                __func__, request->target_chanwidth);
        return -EINVAL;
    }

    if (IEEE80211_IS_CHAN_11AXG(ch) && (request->target_chanwidth > WIFI_OPERATING_CHANWIDTH_40MHZ)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: chan width %d requested for 11AXG mode:\n",
                __func__, request->target_chanwidth);
        return -EINVAL;
    }

    return 0;
}

int ieee80211_extended_ioctl_rep_move (struct net_device *dev,
                                       struct ieee80211com *ic,
                                       caddr_t param)
{
    wifi_repeater_move_request_t *request = (wifi_repeater_move_request_t *)param;
    wifi_channel_switch_request_t request_cswitch;
    ieee80211_vap_t vap = NULL;
    uint8_t nacvaps = 0;
    int error = 0;
    int freq = 0;
    struct ieee80211vap *sta_vap = NULL;
    struct ieee80211_ath_channel *ch  = NULL;

    if (!ic || !param) {
        return -EFAULT;
    }

    sta_vap = ic->ic_sta_vap;

    if (!sta_vap) {
        return -EAGAIN;
    }

    if (ic->ic_strict_doth && ic->ic_non_doth_sta_cnt) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: strict_doth is enabled and non_doth_sta_cnt is %d"
                " Discarding repeater move request\n",
                __func__, ic->ic_non_doth_sta_cnt);
        return -EAGAIN;
    }

    if (wlan_get_param(sta_vap, IEEE80211_FEATURE_VAP_ENHIND)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                             "%s: Enhanced Independent mode is enabled"
                             " Discarding repeater move request\n",
                             __func__);
        return -EAGAIN;
    }

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
            "%s: for %s:\n", __func__, dev->name);

    /* Ensure previous channel switch is not pending */
    if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: Channel change already in progress\n", __func__);
        return -EBUSY;
    }

    /* Iterate VAP list and obtain the AP VAP if present */
    vap = wlan_get_ap_mode_vap(ic);

    /* Number of active vaps on the radio */
    nacvaps = ieee80211_get_num_active_vaps(ic);

    if (!nacvaps || (vap == NULL)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: No active vap present on radio\n", __func__);
        return -EAGAIN;
    }

    request_cswitch = request->chan_switch_req;
    ch = ic->ic_curchan;

    /* Set channel width to current channel width incase of no channel width information
     * provided by user
     */
    if (request_cswitch.target_chanwidth == WIFI_OPERATING_CHANWIDTH_MAX_ELEM) {
        request_cswitch.target_chanwidth = ieee80211_get_cwm_width_from_channel(ch);
    }

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
            "%s: received req: chanwidth: %d, chan: %d, cfreq2: %d, secchanoff: %d,"
            " num_csa: %d, ssid: %s, ssid_len: %d\n", __func__,
            request_cswitch.target_chanwidth,
            request_cswitch.target_pchannel,
            request_cswitch.target_cfreq2,
            request_cswitch.sec_chan_offset,
            request_cswitch.num_csa,
            request->target_ssid.ssid, request->target_ssid.ssid_len);

    if (ieee80211_check_chan_switch_sanity(ic, ch, &request_cswitch)) {
        return -EINVAL;
    }

    freq = ieee80211_ieee2mhz(ic, request_cswitch.target_pchannel, 0);

    error = ieee80211_trigger_channel_switch(ic, &request_cswitch);

    if (!error) {
        /* Repeater move is triggered only for the following cases -
         *
         * 1. Between Root APs with different SSIDs having same or different
         *    channel
         * 2. Between Root APs with same SSID and channel only if
         *    target Root AP BSSID information is provided
         */
        if (ic->ic_repeater_move.same_chan &&
            !qdf_mem_cmp(sta_vap->iv_essid, request->target_ssid.ssid, request->target_ssid.ssid_len)) {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                                     "%s: Same channel and SSID specified:\n",
                                      __func__);
                if(IEEE80211_ADDR_EQ(sta_vap->iv_bss->ni_bssid, request->target_bssid)) {
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                                         "%s: Same BSSID specified:\n",
                                         __func__);
                    return -EINVAL;
                } else if (!IEEE80211_ADDR_IS_VALID(request->target_bssid)) {
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                                         "%s: Invalid BSSID specified:\n",
                                         __func__);
                    return -EINVAL;
                }
        }

        /* Update repeater move state to START once we reach here.
         * Update channel information for repeater move.
         */
        ic->ic_repeater_move.state = REPEATER_MOVE_START;
        ic->ic_repeater_move.chan = freq;

        /* Copy SSID information of new Root AP into repeater move structure
         * maintained in ic. This will be used to expedite scan process
         */
        ic->ic_repeater_move.ssid.len = request->target_ssid.ssid_len;
        qdf_mem_copy(ic->ic_repeater_move.ssid.ssid, request->target_ssid.ssid, request->target_ssid.ssid_len);

        /* Copy BSSID information of new Root AP into repeater move structure
         * maintained in ic. This will be used to expedite scan process
         */
        qdf_mem_copy(ic->ic_repeater_move.bssid, request->target_bssid, sizeof(request->target_bssid));

        /* In case of repeater move trigger in the same channel with
         * different SSID, then we will not have CSA but continue with
         * rest of the process as it is. Since there will be no VAP
         * restart in this case, we directly deliver forced beacon miss
         * to achieve faster disconnect and move to new Root AP
         */
         if (ic->ic_repeater_move.same_chan) {
                IEEE80211_DELIVER_EVENT_BEACON_MISS(sta_vap);
                ic->ic_repeater_move.state = REPEATER_MOVE_IN_PROGRESS;
         }
    }

    return error;
}

/**
 * @brief    common ioctl handler for both direct attach and offload architecture
 *           validates channel switch request and invokes channel switch routine.
 * @param    dev     struct net_device* associated with wifiX radio
 * @param    ic      struct ieee80211com * associated with dev
 * @param    param   pointer carrying request information.
 *
 * @return   0 if parameters are correct and channel switch triggered successfully
 *           else returns negative error code.
 *
 */
int ieee80211_extended_ioctl_chan_switch (struct net_device *dev,
        struct ieee80211com *ic,
        caddr_t param)
{

    wifi_channel_switch_request_t *request = (wifi_channel_switch_request_t *)param;
    struct ieee80211_ath_channel *ch  = NULL;
    u_int32_t nacvaps             = 0;
    int                     error = 0;

    if (!ic || !param) {
        return -EFAULT;
    }

    if (ic->ic_strict_doth && ic->ic_non_doth_sta_cnt) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: strict_doth is enabled and non_doth_sta_cnt is %d"
                " Discarding channel switch request\n",
                __func__, ic->ic_non_doth_sta_cnt);
        return -EAGAIN;
    }

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
            "%s: for %s:\n", __func__, dev->name);

    /* Ensure previous channel switch is not pending */
    if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: Channel change already in progress\n", __func__);
        return -EBUSY;
    }

    nacvaps = ieee80211_get_num_active_vaps(ic);
    if (!nacvaps) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
                "%s: Error: No active vap present on radio\n", __func__);
        return -EAGAIN;
    }

    /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on channel switching
     * rules */

    /* verifying each input parameter for correctness may become costly.
     * only basic sanity is performed here.
     */

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSWITCH,
            "%s: received req: chanwidth: %d, chan: %d, cfreq2: %d, secchanoff: %d,"
            " num_csa: %d\n", __func__, request->target_chanwidth,
            request->target_pchannel, request->target_cfreq2,
            request->sec_chan_offset, request->num_csa);

    ch = ic->ic_curchan;

    /* Set channel width to current channel width incase of no channel width information
     * provided by user
     */
    if (request->target_chanwidth == WIFI_OPERATING_CHANWIDTH_MAX_ELEM) {
        request->target_chanwidth = ieee80211_get_cwm_width_from_channel(ch);
    }

    if (ieee80211_check_chan_switch_sanity(ic, ch, request)) {
        return -EINVAL;
    }

    error = ieee80211_trigger_channel_switch(ic, request);

    return error;
}

QDF_STATUS
ieee80211_update_scan_channel_phymode (struct ieee80211vap *vap,
                                       ieee80211_chanlist_t *chanlist,
                                       struct scan_start_request *scan_params)
{
    enum ieee80211_mode mode_order[] = {IEEE80211_MODE_AXA, IEEE80211_MODE_AXG,
        IEEE80211_MODE_AC, IEEE80211_MODE_NA, IEEE80211_MODE_NG, IEEE80211_MODE_A,
        IEEE80211_MODE_G};
    uint32_t freq_list[CHAN_SCAN_MAX] = {0};
    uint32_t phymode_list[CHAN_SCAN_MAX] = {0};
    uint32_t chan_idx;
    enum ieee80211_phymode phymode;
    struct ieee80211_ath_channel *pchan;
    struct ieee80211com *ic = vap->iv_ic;
    int i = 0, idx = 0;
    int max_phymode_idx = sizeof(mode_order) / sizeof(mode_order[0]);
    uint8_t chan_width;
    bool is_2gvht_en;
    uint8_t sec_chan_offset = IEEE80211_SEC_CHAN_OFFSET_SCN;

    for (i = 0; i < chanlist->n_chan; i++) {
        chan_idx = chanlist->chan[i];
        chan_width = chanlist->chan_width[i];
        sec_chan_offset = chanlist->sec_chan_offset[i];
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                    IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                    "chan_idx: %d, chan_width: %d\n", chan_idx, chan_width);
        if (chan_width > WIFI_OPERATING_CHANWIDTH_160MHZ) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                    IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                    "%s scan width %d not allowed\n", __func__, chan_width);
            return QDF_STATUS_E_INVAL;
        }
        idx = 0;
        do {
            if (sec_chan_offset) {
                phymode = ieee80211_get_composite_phymode(mode_order[idx],
                                                          chan_width, sec_chan_offset);
                pchan = ieee80211_find_dot11_channel(ic, chan_idx, 0, phymode);
            } else {
                phymode = ieee80211_get_composite_phymode(mode_order[idx],
                             chan_width,IEEE80211_SEC_CHAN_OFFSET_SCA);
                pchan = ieee80211_find_dot11_channel(ic, chan_idx, 0, phymode);
                if (!pchan  && (chan_width == WIFI_OPERATING_CHANWIDTH_40MHZ)) {
                    /* Attempt to find 40- channel */
                   phymode = ieee80211_get_composite_phymode(mode_order[idx], chan_width,
                              IEEE80211_SEC_CHAN_OFFSET_SCB);
                   pchan = ieee80211_find_dot11_channel(ic, chan_idx, 0, phymode);
                }
            }
             IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
             IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                    "%s channel %s for chan_idx: %d, chan_width: %d, phymode: %d\n",
                    __func__, (pchan ? "found" : "not_found"), chan_idx,
                    chan_width, phymode);
            if (pchan)
                break;
            idx++;
        } while (idx < max_phymode_idx);
        if (!pchan) {
            return QDF_STATUS_E_INVAL;
        }
        freq_list[i] = pchan->ic_freq;
        is_2gvht_en = ieee80211_vap_256qam_is_set(vap);
        phymode_list[i] = wlan_get_target_phymode(ic, phymode, is_2gvht_en);
    }

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
            IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
            "%s %d channels provided for wide band scan\n",
            __func__, chanlist->n_chan);

    for (i = 0; i < chanlist->n_chan; i++)
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "Freq: %d, target_phymode: %d", freq_list[i],
                phymode_list[i]);

    return ucfg_scan_init_chanlist_params(scan_params, chanlist->n_chan,
            &freq_list[0], &phymode_list[0]);
}

/**
 * @brief    trigger wide band scan
 * @param    vap     pointer to struct ieee80211vap
 * @param    request request parameters
 *
 * @return   0 if wide band scan triggered successfully, false otherwise.
 */
static int
ieee80211_trigger_wide_band_scan(struct ieee80211vap *vap,
        wifi_scan_custom_request_cmd_t *request)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct scan_start_request *scan_params;
    IEEE80211_SCAN_PRIORITY priority;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_scan_id scan_id;
    osif_dev *osifp = (osif_dev *)vap->iv_ifp;

    if (!request->chanlist.n_chan) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "%s Zero channels provided", __func__);
        return -EINVAL;
    }

    vdev = vap->vdev_obj;
    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "%s unable to get reference", __func__);
        return -EINVAL;
    }

    scan_params = (struct scan_start_request *)
        qdf_mem_malloc(sizeof(*scan_params));
    if (scan_params == NULL) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "%s unable to allocate scan request", __func__);
        return -EINVAL;
    }
    OS_MEMZERO(scan_params,sizeof(*scan_params));
    status = wlan_update_scan_params(vap, scan_params, wlan_vap_get_opmode(vap),
            true,true,false,true,0,NULL,0);
    if (status) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        qdf_mem_free(scan_params);
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "%s scan param init failed with status: %d", __func__, status);
        return -EINVAL;
    }

    scan_params->scan_req.dwell_time_active =
        scan_params->scan_req.dwell_time_passive = request->min_dwell_time;

    scan_params->scan_req.min_rest_time =
        scan_params->scan_req.max_rest_time = request->rest_time;

    priority = SCAN_PRIORITY_HIGH;
    scan_params->scan_req.scan_flags = 0;
    scan_params->scan_req.scan_f_passive =
        (request->scan_mode == IEEE80211_SCAN_ACTIVE) ?
        false : true;
    scan_params->scan_req.scan_f_2ghz = true;
    scan_params->scan_req.scan_f_5ghz = true;
    scan_params->scan_req.scan_f_bcast_probe = true;

    status = ieee80211_update_scan_channel_phymode(vap, &(request->chanlist), scan_params);

    if (QDF_IS_STATUS_ERROR(status)) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        qdf_mem_free(scan_params);
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "%s update_scan_channel_phymode failed with status: %d",
                __func__, status);
        return -EINVAL;
    }

    status = wlan_ucfg_scan_start(vap, scan_params, osifp->scan_requestor,
            priority, &scan_id, 0, NULL);
    if (QDF_IS_STATUS_ERROR(status)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "%s scan_start failed with status: %d", __func__, status);
    }

    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);

    return status;
}

/**
 * @brief    common ioctl handler for both direct attach and offload architecture
 *           set all user configured parameters and start the scan.
 * @param    dev     struct net_device* associated with wifiX radio
 * @param    ic      struct ieee80211com * associated with dev
 * @param    param   pointer carrying request information.
 *
 * @return   0 if parameters are correct and channel scan
 *           else returns negative error code.
 *
 */
int ieee80211_extended_ioctl_chan_scan (struct net_device *dev,
        struct ieee80211com *ic,
        caddr_t param)
{
    //wifi_scan_custom_request_cmd_t old_config;
    wifi_scan_custom_request_cmd_t *request = (wifi_scan_custom_request_cmd_t *)param;
    struct ieee80211vap *vap = NULL;
    ieee80211_chanlist_t *list = NULL;
    u_int32_t val = 1;
    int error = 0;
    int has_active_vap = 0;

    if (!(dev->flags & IFF_UP)) {
        return -EINVAL;
    }

    if (!ic || !param) {
        return -EFAULT;
    }

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap) {
            if (wlan_vdev_chan_config_valid(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
                has_active_vap = 1;
                break;
            }
        }
    }

    if (!vap || !has_active_vap) {
        return -EINVAL;
    }


    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
		    "%s: for %s:\n", __func__, dev->name);

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
        "%s: received req: max_dwell_time: %d, min_dwell_time: %d, scan_mode: %d, rest_time: %d,"
        "idle_time: %d, max_scan_time: %d\n", __func__, request->max_dwell_time,
        request->min_dwell_time, request->scan_mode, request->rest_time,
        request->idle_time, request->max_scan_time);

    /* Setting user preferred channel for scanning */
    list = &request->chanlist;
    if (list == NULL) {
	    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN, "%s user channel list become null", __func__);
	    return -EINVAL;
    }

    if (request->wide_band) {
        if (wlan_wide_band_scan_enabled(ic)) {
            return ieee80211_trigger_wide_band_scan(vap, request);
        } else {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                    "%s Wide band scan not enabled/supported", __func__);
            return -EINVAL;
        }
    }

    if (wlan_acs_set_user_chanlist(vap,list->chan) != EOK) {
	    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN, "%s Failed to set user channel", __func__);
	    return -EINVAL;
    }

    /* Setting the user configured max dwell time */
    if (wlan_acs_start_scan_report(vap, 1, IEEE80211_MAX_DWELL_ACS_REPORT, (void *)&request->max_dwell_time) != EOK) {
	    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN, "%s Failed to set max dwell time", __func__);
	    return -EINVAL;
    }

    /* Setting the user configured min dwell time */
    if (wlan_acs_start_scan_report(vap, 1, IEEE80211_MIN_DWELL_ACS_REPORT, (void *)&request->min_dwell_time) != EOK) {
	    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN, "%s Failed to set min dwell time", __func__);
	    return -EINVAL;
    }

    /* Setting the user configured rest time */
    if (wlan_acs_start_scan_report(vap, 1, IEEE80211_SCAN_REST_TIME, (void *)&request->rest_time) != EOK) {
	    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN, "%s Failed to set rest time", __func__);
	    return -EINVAL;
    }

    /* Setting the user configured scan mode */
    if (wlan_acs_start_scan_report(vap, 1, IEEE80211_SCAN_MODE, (void *)&request->scan_mode) != EOK) {
	    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN, "%s Failed to set scan_mode", __func__);
	    return -EINVAL;
    }

    /* starting the ACS scan */
    if (wlan_acs_start_scan_report(vap, 1, IEEE80211_START_ACS_REPORT, (void *)&val) != EOK) {
	    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_EXTIOCTL_CHANSSCAN, "%s Failed to start ACS scan", __func__);
	    return -EINVAL;
    }
    return error;
}

