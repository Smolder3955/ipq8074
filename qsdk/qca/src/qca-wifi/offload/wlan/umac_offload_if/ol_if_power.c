/*
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ol_if_athvar.h"
#include "qdf_mem.h"
#include <init_deinit_lmac.h>
#include <wlan_vdev_mgr_ucfg_api.h>
#if ATH_PERF_PWR_OFFLOAD

#define IEEE80211_PS_INACTIVITYTIME               400 /* pwrsave inactivity time in sta mode (in msec) */

struct ieee80211_sta_power {
    int fullsleep_enable; /* not used */

    u_int32_t max_sleeptime;
    u_int32_t norm_sleeptime;
    u_int32_t low_sleeptime;

    u_int32_t max_inactivitytime;
    u_int32_t norm_inactivitytime;
    u_int32_t low_inactivitytime;
    u_int32_t inactivity_time; /* current */

    enum wmi_host_sta_ps_mode psmode;
    u_int32_t force_sleep;
    u_int32_t pspoll_enabled;
    ieee80211_pspoll_moredata_handling pspoll_moredata;
};

int
wmi_unified_set_ap_ps_param(
        struct ol_ath_vap_net80211 *avn,
        struct ol_ath_node_net80211 *anode,
        A_UINT32 param,
        A_UINT32 value)
{
    struct ol_ath_softc_net80211 *svn = avn->av_sc;
    struct ap_ps_params ps_param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(svn->sc_pdev);
    qdf_mem_set(&ps_param, sizeof(ps_param), 0);
    ps_param.vdev_id = avn->av_if_id;
    ps_param.param = param;
    ps_param.value = value;
    return wmi_unified_ap_ps_cmd_send(pdev_wmi_handle, anode->an_node.ni_macaddr,
        &ps_param);
}

int
wmi_unified_set_sta_ps_param(
        struct ol_ath_vap_net80211 *avn,
        A_UINT32 param,
        A_UINT32 value)
{
    struct ol_ath_softc_net80211 *svn = avn->av_sc;
    struct sta_ps_params ps_param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(svn->sc_pdev);
    qdf_mem_set(&ps_param, sizeof(ps_param), 0);
    ps_param.vdev_id = avn->av_if_id;
    ps_param.param_id = param;
    ps_param.value = value;

    return wmi_unified_sta_ps_cmd_send(pdev_wmi_handle, &ps_param);
}

static int
wmi_unified_set_psmode(
        struct ol_ath_vap_net80211 *avn,
        ieee80211_pwrsave_mode psmode)
{
    struct ol_ath_softc_net80211 *svn = avn->av_sc;
    struct set_ps_mode_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(svn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = avn->av_if_id;
    param.psmode = psmode;

    return wmi_unified_set_psmode_cmd_send(pdev_wmi_handle, &param);
}

int
ol_power_sta_set_pspoll(
        struct ieee80211vap *vap,
        u_int32_t pspoll)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;
    A_UINT32 value;
    int err = 0;

    if (!sta_handle) {
        return EINVAL;
    }

    sta_handle->pspoll_enabled = pspoll;

    if (!sta_handle->force_sleep) {
        if (sta_handle->pspoll_enabled) {
            value = WMI_HOST_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
        } else {
            value = WMI_HOST_STA_PS_RX_WAKE_POLICY_WAKE;
        }

        err = wmi_unified_set_sta_ps_param(OL_ATH_VAP_NET80211(vap),
                WMI_HOST_STA_PS_PARAM_RX_WAKE_POLICY, value);
    }

    return err;
}

int
ol_power_sta_set_pspoll_moredata_handling(
        struct ieee80211vap *vap,
        ieee80211_pspoll_moredata_handling mode)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;
    A_UINT32 value;
    int err = 0;

    if (!sta_handle) {
        return EINVAL;
    }

    sta_handle->pspoll_moredata = mode;

    if (!sta_handle->force_sleep) {

        switch (sta_handle->pspoll_moredata) {
        case IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA:
            value = WMI_HOST_STA_PS_PSPOLL_COUNT_NO_MAX;
            break;
        case IEEE80211_WAKEUP_FOR_MORE_DATA:
            value = 1;
            break;
        default:
            value = WMI_HOST_STA_PS_PSPOLL_COUNT_NO_MAX;
            break;
        }

        err = wmi_unified_set_sta_ps_param(OL_ATH_VAP_NET80211(vap),
                WMI_HOST_STA_PS_PARAM_PSPOLL_COUNT, value);
    }

    return err;
}

u_int32_t
ol_power_sta_get_pspoll(
        struct ieee80211vap *vap)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;

    if (sta_handle) {
        return sta_handle->pspoll_enabled;
    }

    return 0;
}

ieee80211_pspoll_moredata_handling
ol_power_sta_get_pspoll_moredata_handling(
        struct ieee80211vap *vap)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;

    if (sta_handle) {
        return sta_handle->pspoll_moredata;
    }

    return IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA;
}

int
ol_power_sta_set_mode(
        struct ieee80211vap *vap,
        ieee80211_pwrsave_mode mode)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = vap->iv_bss;
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;
    int err = 0;
    u_int16_t lintval;
    enum wmi_host_sta_ps_mode psmode = WMI_HOST_STA_PS_MODE_DISABLED;

    if (!sta_handle || !ni) {
        return EINVAL;
    }

    if (mode == IEEE80211_PWRSAVE_NONE) {
        ieee80211com_clear_flags(ic, IEEE80211_F_PMGTON);
        lintval = 1;
        psmode = WMI_HOST_STA_PS_MODE_DISABLED;
    } else {
        ieee80211com_set_flags(ic, IEEE80211_F_PMGTON);
        if (mode == IEEE80211_PWRSAVE_LOW) {
            lintval = sta_handle->low_sleeptime / ic->ic_intval;
            sta_handle->inactivity_time = sta_handle->low_inactivitytime;
        } else if (mode == IEEE80211_PWRSAVE_NORMAL) {
            lintval = sta_handle->norm_sleeptime / ic->ic_intval;
            sta_handle->inactivity_time = sta_handle->norm_inactivitytime;
        } else {
            lintval = sta_handle->max_sleeptime / ic->ic_intval;
            sta_handle->inactivity_time = sta_handle->max_inactivitytime;
        }
        psmode = WMI_HOST_STA_PS_MODE_ENABLED;
    }

    /*
     * use the max of user configured value and
     * listen interval based on power save
     */
    lintval =(ic->ic_lintval > lintval) ? ic->ic_lintval : lintval;
    ni->ni_lintval = lintval;
    sta_handle->psmode = psmode;

    if (!sta_handle->force_sleep) {
        struct ol_ath_vap_net80211 *avn;
        struct ol_ath_softc_net80211 *scn;
        struct wlan_vdev_mgr_cfg mlme_cfg;
        avn = OL_ATH_VAP_NET80211(vap);
        scn = avn->av_sc;

        err = wmi_unified_set_sta_ps_param(avn,
                WMI_HOST_STA_PS_PARAM_INACTIVITY_TIME, sta_handle->inactivity_time);
        if (err) {
            goto error;
        }

        err = wmi_unified_set_psmode(avn,
                sta_handle->psmode);
        if (err) {
            goto error;
        }

        mlme_cfg.value = lintval;
        ucfg_wlan_vdev_mgr_set_param(vap->vdev_obj,
                WLAN_MLME_CFG_LISTEN_INTERVAL, mlme_cfg);
        if (err) {
            goto error;
        }

    }

error:
    return err;
}

ieee80211_pwrsave_mode
ol_power_sta_get_mode(
        struct ieee80211vap *vap)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;
    ieee80211_pwrsave_mode mode = IEEE80211_PWRSAVE_NONE;

    if (sta_handle) {
        if (sta_handle->psmode == WMI_HOST_STA_PS_MODE_DISABLED) {
            mode = IEEE80211_PWRSAVE_NONE;
        } else if (sta_handle->inactivity_time == sta_handle->low_inactivitytime) {
            mode = IEEE80211_PWRSAVE_LOW;
        } else if (sta_handle->inactivity_time == sta_handle->norm_inactivitytime) {
            mode = IEEE80211_PWRSAVE_NORMAL;
        } else {
            mode = IEEE80211_PWRSAVE_MAXIMUM;
        }
    }

    return mode;
}

int
ol_power_sta_set_inactive_time(
        struct ieee80211vap *vap,
        ieee80211_pwrsave_mode mode,
        u_int32_t inactive_time)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;
    int err = 0;

    if (!sta_handle) {
        return EINVAL;
    }

    switch (mode) {
    case IEEE80211_PWRSAVE_LOW:
        sta_handle->low_inactivitytime = inactive_time;
        break;
    case IEEE80211_PWRSAVE_NORMAL:
        sta_handle->norm_inactivitytime = inactive_time;
        err = wmi_unified_set_sta_ps_param(OL_ATH_VAP_NET80211(vap),
                WMI_HOST_STA_PS_PARAM_INACTIVITY_TIME,
                sta_handle->norm_inactivitytime);
        break;
    case IEEE80211_PWRSAVE_MAXIMUM:
        sta_handle->max_inactivitytime = inactive_time;
        break;
    default:
        break;
    }

    return err;
}

u_int32_t
ol_power_sta_get_inactive_time(
        struct ieee80211vap *vap,
        ieee80211_pwrsave_mode mode)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;
    u_int32_t val = 0;

    if (!sta_handle) {
        return 0;
    }

    switch (mode) {
    case IEEE80211_PWRSAVE_LOW:
        val = sta_handle->low_inactivitytime;
        break;
    case IEEE80211_PWRSAVE_NORMAL:
        val = sta_handle->norm_inactivitytime;
        break;
    case IEEE80211_PWRSAVE_MAXIMUM:
        val = sta_handle->max_inactivitytime;
        break;
    default:
        break;
    }

    return val;
}

int
ol_power_sta_force_sleep(
        struct ieee80211vap *vap,
        bool enable)
{
    ieee80211_sta_power_t sta_handle = vap->iv_pwrsave_sta;
    int err = 0;
    A_UINT32 rx_wake_policy;
    A_UINT32 tx_wake_threshold;
    A_UINT32 pspoll_count;
    A_UINT32 inactivity_time;
    A_UINT32 psmode;

    if (!sta_handle) {
        return EINVAL;
    }

    sta_handle->force_sleep = enable;

    if (enable) {
        /* override normal configuration and force station asleep */
        rx_wake_policy = WMI_HOST_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
        tx_wake_threshold = WMI_HOST_STA_PS_TX_WAKE_THRESHOLD_NEVER;
        pspoll_count = WMI_HOST_STA_PS_PSPOLL_COUNT_NO_MAX;
        inactivity_time = 0;
        psmode = WMI_HOST_STA_PS_MODE_ENABLED;
    } else {
        /* restore previous power save settings */
        if (sta_handle->pspoll_enabled) {
            rx_wake_policy = WMI_HOST_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;

            switch (sta_handle->pspoll_moredata) {
            case IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA:
                pspoll_count = WMI_HOST_STA_PS_PSPOLL_COUNT_NO_MAX;
                break;
            case IEEE80211_WAKEUP_FOR_MORE_DATA:
                pspoll_count = 1;
                break;
            default:
                pspoll_count = WMI_HOST_STA_PS_PSPOLL_COUNT_NO_MAX;
            }
        } else {
            rx_wake_policy = WMI_HOST_STA_PS_RX_WAKE_POLICY_WAKE;
            pspoll_count = WMI_HOST_STA_PS_PSPOLL_COUNT_NO_MAX;
        }

        tx_wake_threshold = WMI_HOST_STA_PS_TX_WAKE_THRESHOLD_ALWAYS;
        psmode = sta_handle->psmode;
        inactivity_time = sta_handle->inactivity_time;
    }

    err = wmi_unified_set_sta_ps_param(OL_ATH_VAP_NET80211(vap),
            WMI_HOST_STA_PS_PARAM_RX_WAKE_POLICY, rx_wake_policy);
    if (err) {
        goto error;
    }

    err = wmi_unified_set_sta_ps_param(OL_ATH_VAP_NET80211(vap),
            WMI_HOST_STA_PS_PARAM_TX_WAKE_THRESHOLD, tx_wake_threshold);
    if (err) {
        goto error;
    }

    err = wmi_unified_set_sta_ps_param(OL_ATH_VAP_NET80211(vap),
            WMI_HOST_STA_PS_PARAM_PSPOLL_COUNT, pspoll_count);
    if (err) {
        goto error;
    }

    err = wmi_unified_set_sta_ps_param(OL_ATH_VAP_NET80211(vap),
            WMI_HOST_STA_PS_PARAM_INACTIVITY_TIME, inactivity_time);
    if (err) {
        goto error;
    }

    err = wmi_unified_set_psmode(OL_ATH_VAP_NET80211(vap), psmode);
    if (err) {
        goto error;
    }

    return 0;
error:
    /* TODO rollback changes? */
    return err;
}

static ieee80211_sta_power_t
ol_power_sta_vattach(
        struct ieee80211vap *vap,
        int                 fullsleep_enable,
        u_int32_t           max_sleeptime,
        u_int32_t           norm_sleeptime,
        u_int32_t           low_sleeptime,
        u_int32_t           max_inactivitytime,
        u_int32_t           norm_inactivitytime,
        u_int32_t           low_inactivitytime,
        u_int32_t           pspoll_enabled)
{
    ieee80211_sta_power_t    sta_handle;
    osdev_t                  os_handle = vap->iv_ic->ic_osdev;

    sta_handle = (ieee80211_sta_power_t)OS_MALLOC(os_handle,
            sizeof(struct ieee80211_sta_power), 0);
    if (!sta_handle) {
        return NULL;
    }

    /* Power save is already initialized when FW VAP is created - we just need
     * to configure it.
     */
    sta_handle->fullsleep_enable = fullsleep_enable;
    sta_handle->max_sleeptime = max_sleeptime;
    sta_handle->norm_sleeptime = norm_sleeptime;
    sta_handle->low_sleeptime = low_sleeptime;
    sta_handle->max_inactivitytime = max_inactivitytime;
    sta_handle->norm_inactivitytime = norm_inactivitytime;
    sta_handle->low_inactivitytime = low_inactivitytime;
    sta_handle->force_sleep = false;
    sta_handle->pspoll_enabled = pspoll_enabled;
    sta_handle->pspoll_moredata = IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA;
    sta_handle->psmode = WMI_HOST_STA_PS_MODE_ENABLED;
    sta_handle->inactivity_time = IEEE80211_PS_INACTIVITYTIME;

    return sta_handle;
}

void
ol_power_sta_vdetach(
        ieee80211_sta_power_t sta_handle)
{
    OS_FREE(sta_handle);
}

/*********************************************************************/

static int
ol_power_ap_alloc_tim_bitmap(
        struct ieee80211vap *vap)
{
    u_int8_t *tim_bitmap = NULL;
    u_int16_t old_len = vap->iv_tim_len;

    vap->iv_tim_len = howmany(vap->iv_max_aid, 8) * sizeof(u_int8_t);
    tim_bitmap = OS_MALLOC(vap->iv_ic->ic_osdev, MAX_TIM_BITMAP_LENGTH , 0);
    if(!tim_bitmap) {
        vap->iv_tim_len = old_len;
        return -1;
    }
    OS_MEMZERO(tim_bitmap, vap->iv_tim_len);
    if (vap->iv_tim_bitmap) {
        OS_MEMCPY(tim_bitmap, vap->iv_tim_bitmap,
            vap->iv_tim_len > old_len ? old_len : vap->iv_tim_len);
        OS_FREE(vap->iv_tim_bitmap);
    }
    vap->iv_tim_bitmap = tim_bitmap;

    return 0;
}

void
ol_power_ap_set_tim(struct ieee80211_node *ni, int set, bool isr_context)
{
}

void
ol_power_ap_vattach(struct ieee80211vap *vap)
{
    /* The target FW updates TIM and includes the latest TIM info along with
     * WMI_UNIFIED_HOST_SWBA_EVENTID */
    vap->iv_set_tim = ol_power_ap_set_tim;
    vap->iv_alloc_tim_bitmap = ol_power_ap_alloc_tim_bitmap;

    /*
     * Allocate these only if needed
     */
    if (!vap->iv_tim_bitmap) {
        vap->iv_tim_len = howmany(vap->iv_max_aid,8) * sizeof(u_int8_t);
        vap->iv_tim_bitmap = (u_int8_t *) OS_MALLOC(vap->iv_ic->ic_osdev, MAX_TIM_BITMAP_LENGTH , 0);
        if (vap->iv_tim_bitmap == NULL) {
            printf("%s: no memory for TIM bitmap!\n", __func__);
            vap->iv_tim_len = 0;
        } else {
            OS_MEMZERO(vap->iv_tim_bitmap,vap->iv_tim_len);
        }
    }

    /* WMI defaults */
}

void
ol_power_ap_vdetach(
        struct ieee80211vap *vap)
{
    if (vap->iv_tim_bitmap != NULL) {
        OS_FREE(vap->iv_tim_bitmap);
        vap->iv_tim_bitmap = NULL;
        vap->iv_tim_len = 0;
        vap->iv_ps_sta = 0;
    }
}

/*********************************************************************/

static void
ol_power_vattach(
        struct ieee80211vap *vap,
        int fullsleepEnable,
        u_int32_t sleepTimerPwrSaveMax,
        u_int32_t sleepTimerPwrSave,
        u_int32_t sleepTimePerf,
        u_int32_t inactTimerPwrsaveMax,
        u_int32_t inactTimerPwrsave,
        u_int32_t inactTimerPerf,
        u_int32_t smpsDynamic,
        u_int32_t pspollEnabled)
{
    vap->iv_power = NULL;

    switch (vap->iv_opmode)  {
    case IEEE80211_M_HOSTAP:
        ol_power_ap_vattach(vap);
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL;
        break;
    case IEEE80211_M_STA:
        vap->iv_pwrsave_sta = ol_power_sta_vattach(vap, fullsleepEnable,
                sleepTimerPwrSaveMax, sleepTimerPwrSave, sleepTimePerf,
                inactTimerPwrsaveMax, inactTimerPwrsave, inactTimerPerf,
                pspollEnabled);

        /* This has the effect of restoring the current configuration and
         * writing it to target FW via WMI */
        (void)ol_power_sta_force_sleep(vap, false);

        vap->iv_pwrsave_smps = NULL; /* TODO */
        break;
    default:
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL;
    }
}


static void
ol_power_vdetach(
        struct ieee80211vap *vap)
{
    switch (vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
        ol_power_ap_vdetach(vap);
        break;
    case IEEE80211_M_STA:
        ol_power_sta_vdetach(vap->iv_pwrsave_sta);
        vap->iv_pwrsave_sta = NULL;
        vap->iv_pwrsave_smps = NULL;
        break;
    default:
        break;
    }
}

static void
ol_power_detach(
        struct ieee80211com *ic)
{
    ic->ic_power_sta_set_pspoll = NULL;
    ic->ic_power_sta_set_pspoll_moredata_handling = NULL;
    ic->ic_power_sta_get_pspoll = NULL;
    ic->ic_power_sta_get_pspoll_moredata_handling = NULL;
    ic->ic_power_set_mode = NULL;
    ic->ic_power_get_mode = NULL;
    ic->ic_power_get_apps_state = NULL;
    ic->ic_power_set_inactive_time = NULL;
    ic->ic_power_get_inactive_time = NULL;
    ic->ic_power_force_sleep = NULL;
    ic->ic_power_set_ips_pause_notif_timeout = NULL;
    ic->ic_power_get_ips_pause_notif_timeout = NULL;
    ic->ic_power_detach = NULL;
    ic->ic_power_vattach = NULL;
    ic->ic_power_vdetach = NULL;
}

static void
ol_power_attach(
        struct ieee80211com *ic)
{
    ic->ic_power_sta_set_pspoll = ol_power_sta_set_pspoll;
    ic->ic_power_sta_set_pspoll_moredata_handling = ol_power_sta_set_pspoll_moredata_handling;
    ic->ic_power_sta_get_pspoll = ol_power_sta_get_pspoll;
    ic->ic_power_sta_get_pspoll_moredata_handling = ol_power_sta_get_pspoll_moredata_handling;
    ic->ic_power_set_mode = ol_power_sta_set_mode;
    ic->ic_power_get_mode = ol_power_sta_get_mode;
    ic->ic_power_get_apps_state = NULL;
    ic->ic_power_set_inactive_time = ol_power_sta_set_inactive_time;
    ic->ic_power_get_inactive_time = ol_power_sta_get_inactive_time;
    ic->ic_power_force_sleep = ol_power_sta_force_sleep;
    ic->ic_power_set_ips_pause_notif_timeout = NULL;
    ic->ic_power_get_ips_pause_notif_timeout = NULL;

    ic->ic_power_sta_tx_start = NULL;
    ic->ic_power_sta_tx_end = NULL;
    ic->ic_power_sta_pause = NULL;
    ic->ic_power_sta_unpause = NULL;
    ic->ic_power_sta_send_keepalive = NULL;
    ic->ic_power_sta_register_event_handler = NULL;
    ic->ic_power_sta_unregister_event_handler = NULL;
    ic->ic_power_sta_event_tim = NULL;
    ic->ic_power_sta_event_dtim = NULL;

    ic->ic_power_detach = ol_power_detach;
    ic->ic_power_vattach = ol_power_vattach;
    ic->ic_power_vdetach = ol_power_vdetach;
}

void
ol_ath_power_attach(
        struct ieee80211com *ic)
{
    ic->ic_power_attach = ol_power_attach;
}
#endif /* ATH_PERF_PWR_OFFLOAD */
