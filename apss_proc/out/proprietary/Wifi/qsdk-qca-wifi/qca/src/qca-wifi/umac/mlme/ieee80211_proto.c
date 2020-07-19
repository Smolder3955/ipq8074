/*
 * Copyright (c) 2011-2017,2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include <osdep.h>

#include <ieee80211_var.h>
#include <ieee80211_api.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include <ieee80211_defines.h>
#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_api.h>
#include <wlan_sa_api_utils_defs.h>
#endif
#include "ieee80211_sme_api.h"
#include <ieee80211_objmgr_priv.h>
#include <wlan_offchan_txrx_api.h>
#include <ieee80211_bsscolor.h>
#include "target_type.h"
#include "include/wlan_vdev_mlme.h"
#include <wlan_vdev_mgr_utils_api.h>
#include <wlan_vdev_mgr_tgt_if_tx_defs.h>

/* XXX tunables */
#define AGGRESSIVE_MODE_SWITCH_HYSTERESIS   3   /* pkts / 100ms */
#define HIGH_PRI_SWITCH_THRESH              10  /* pkts / 100ms */

const char *ieee80211_mgt_subtype_name[] = {
    "assoc_req",    "assoc_resp",   "reassoc_req",  "reassoc_resp",
    "probe_req",    "probe_resp",   "reserved#6",   "reserved#7",
    "beacon",       "atim",         "disassoc",     "auth",
    "deauth",       "action",       "reserved#14",  "reserved#15"
};


const char *ieee80211_wme_acnames[] = {
    "WME_AC_BE",
    "WME_AC_BK",
    "WME_AC_VI",
    "WME_AC_VO",
    "WME_UPSD"
};


void
ieee80211_proto_attach(struct ieee80211com *ic)
{
    ic->ic_protmode = IEEE80211_PROT_CTSONLY;

    ic->ic_wme.wme_hipri_switch_hysteresis =
        AGGRESSIVE_MODE_SWITCH_HYSTERESIS;

    ATH_HTC_NETDEFERFN_INIT(ic);


#if 0  /* XXX TODO */
    ieee80211_auth_setup();
#endif
}

void
ieee80211_proto_detach(struct ieee80211com *ic)
{

    ATH_HTC_NETDEFERFN_CLEANUP(ic);
}

void ieee80211_proto_vattach(struct ieee80211vap *vap)
{
    vap->iv_fixed_rate.mode = IEEE80211_FIXED_RATE_NONE;

}

void
ieee80211_proto_vdetach(struct ieee80211vap *vap)
{
    ieee80211_mlme_unregister_event_handler( vap, ieee80211_mlme_event_callback, NULL);
}

void get_sta_mode_vap(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *ppvap=(ieee80211_vap_t *)arg;

    if (IEEE80211_M_STA == vap->iv_opmode) {
        *ppvap = vap;
    }
}

#if UMAC_REPEATER_DELAYED_BRINGUP
void
ieee80211_vap_handshake_finish(struct ieee80211vap *vap)
{
	if (vap->iv_opmode == IEEE80211_M_STA)
		ieee80211_state_event(vap, IEEE80211_STATE_EVENT_HANDSHAKE_FINISH);
}
#endif


/*
 * Reset 11g-related state.
 */
void
ieee80211_reset_erp(struct ieee80211com *ic,
                    enum ieee80211_phymode mode,
                    enum ieee80211_opmode opmode)
{
#define IS_11G(m) \
    ((m) == IEEE80211_MODE_11G || (m) == IEEE80211_MODE_TURBO_G)

    struct ieee80211_ath_channel *chan = ieee80211_get_current_channel(ic);

    /* 11AX TODO (Phase II) - Check whether updates are required here */

    IEEE80211_DISABLE_PROTECTION(ic);

    /*
     * Preserve the long slot and nonerp station count if
     * switching between 11g and turboG. Otherwise, inactivity
     * will cause the turbo station to disassociate and possibly
     * try to leave the network.
     * XXX not right if really trying to reset state
     */
    if (IS_11G(mode) ^ IS_11G(ic->ic_curmode)) {
        ic->ic_nonerpsta = 0;
        ic->ic_longslotsta = 0;
    }

    /*
     * Short slot time is enabled only when operating in 11g
     * and not in an IBSS.  We must also honor whether or not
     * the driver is capable of doing it.
     */
    if(opmode == IEEE80211_M_IBSS){
        ieee80211_set_shortslottime(ic, 0);
    }else{
        ieee80211_set_shortslottime(
            ic,
            IEEE80211_IS_CHAN_A(chan) ||
            IEEE80211_IS_CHAN_11NA(chan) ||
            IEEE80211_IS_CHAN_11AC(chan) ||
            IEEE80211_IS_CHAN_11AXA(chan) ||
            ((IEEE80211_IS_CHAN_ANYG(chan) ||
              IEEE80211_IS_CHAN_11NG(chan) ||
              IEEE80211_IS_CHAN_11AXG(chan)) &&
             (opmode == IEEE80211_M_HOSTAP || opmode == IEEE80211_M_BTAMP
              || opmode == IEEE80211_M_MONITOR) && (ic->ic_caps & IEEE80211_C_SHSLOT)));
    }

    /*
     * Set short preamble and ERP barker-preamble flags.
     */
    if (IEEE80211_IS_CHAN_A(chan) ||
        IEEE80211_IS_CHAN_11NA(chan) ||
        (ic->ic_caps & IEEE80211_C_SHPREAMBLE)) {
        IEEE80211_ENABLE_SHPREAMBLE(ic);
        IEEE80211_DISABLE_BARKER(ic);
    } else {
        IEEE80211_DISABLE_SHPREAMBLE(ic);
        IEEE80211_ENABLE_BARKER(ic);
    }
#undef IS_11G
}

/*
 * Update the protection mode in all vaps and notify the driver.
 */
void
ieee80211_update_protmode(struct wlan_objmgr_pdev *pdev,
        void *object, void *arg)
{
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
    struct ieee80211com *ic = (struct ieee80211com *)arg;
    struct vdev_mlme_obj *vdev_mlme;
    struct wlan_vdev_mgr_cfg mlme_cfg;

    vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(
                                        vdev,
                                        WLAN_UMAC_COMP_MLME);
    if(!vdev_mlme) {
        QDF_ASSERT(0);
        return;
    }

    mlme_cfg.value = IEEE80211_PROT_NONE;
    if (IEEE80211_IS_PROTECTION_ENABLED(ic)) {
        /* Protection flag is set. Use ic_protmode */
        mlme_cfg.value = ic->ic_protmode;
    }

    wlan_util_vdev_mlme_set_param(vdev_mlme,
                WLAN_MLME_CFG_PROTECTION_MODE, mlme_cfg);
}

/*
 * Set the protection mode and notify the driver.
 */
void
ieee80211_set_protmode(struct ieee80211com *ic)
{
    wlan_objmgr_pdev_iterate_obj_list(
            ic->ic_pdev_obj, WLAN_VDEV_OP,
            ieee80211_update_protmode,
            ic, 0, WLAN_MLME_NB_ID);
}

/*
 * Update the short slot time in all vaps and notify the driver.
 */
void
ieee80211_update_shortslottime(struct wlan_objmgr_pdev *pdev,
        void *object, void *arg)
{
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
    struct ieee80211com *ic = (struct ieee80211com *)arg;
    struct vdev_mlme_obj *vdev_mlme;
    struct wlan_vdev_mgr_cfg mlme_cfg;

    vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(
                                        vdev,
                                        WLAN_UMAC_COMP_MLME);
    if(!vdev_mlme) {
        QDF_ASSERT(0);
        return;
    }

    if(IEEE80211_IS_SHSLOT_ENABLED(ic))
        mlme_cfg.value = WLAN_MLME_VDEV_SLOT_TIME_SHORT;
    else
        mlme_cfg.value = WLAN_MLME_VDEV_SLOT_TIME_LONG;

    wlan_util_vdev_mlme_set_param(vdev_mlme,
                WLAN_MLME_CFG_SLOT_TIME, mlme_cfg);
}

/*
 * Set the short slot time state and notify the driver.
 */
void
ieee80211_set_shortslottime(struct ieee80211com *ic, int onoff)
{
    if (onoff)
        IEEE80211_ENABLE_SHSLOT(ic);
    else
        IEEE80211_DISABLE_SHSLOT(ic);

    wlan_objmgr_pdev_iterate_obj_list(
            ic->ic_pdev_obj, WLAN_VDEV_OP,
            ieee80211_update_shortslottime,
            ic, 0, WLAN_MLME_NB_ID);
}

void
ieee80211_wme_initparams(struct ieee80211vap *vap)
{
    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        /*
         * For AP, we need to synchronize with SWBA interrupt.
         * So we need to lock out interrupt.
         */
    //    OS_EXEC_INTSAFE(vap->iv_ic->ic_osdev, ieee80211_wme_initparams_locked, vap);
	if(vap->iv_rescan)
		return;
        ieee80211_wme_initparams_locked(vap);
    } else {
        ieee80211_wme_initparams_locked(vap);
    }
}

void
ieee80211_wme_initparams_locked(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_wme_state *wme = &ic->ic_wme;
    enum ieee80211_phymode mode;
    const wmeParamType *pPhyParam, *pBssPhyParam;
    int i;

    //IEEE80211_BEACON_LOCK_ASSERT(ic);

    if ((ic->ic_caps & IEEE80211_C_WME) == 0)
        return;

    /*
     * Select mode; we can be called early in which case we
     * always use auto mode.  We know we'll be called when
     * entering the RUN state with bsschan setup properly
     * so state will eventually get set correctly
     */
    if (vap->iv_bsschan != IEEE80211_CHAN_ANYC)
        mode = ieee80211_chan2mode(vap->iv_bsschan);
    else
        mode = IEEE80211_MODE_AUTO;
    for (i = 0; i < WME_NUM_AC; i++) {
        switch (i) {
        case WME_AC_BK:
            pPhyParam = &ic->phyParamForAC[WME_AC_BK][mode];
            pBssPhyParam = &ic->phyParamForAC[WME_AC_BK][mode];
            break;
        case WME_AC_VI:
            pPhyParam = &ic->phyParamForAC[WME_AC_VI][mode];
            pBssPhyParam = &ic->bssPhyParamForAC[WME_AC_VI][mode];
            break;
        case WME_AC_VO:
            pPhyParam = &ic->phyParamForAC[WME_AC_VO][mode];
            pBssPhyParam = &ic->bssPhyParamForAC[WME_AC_VO][mode];
            break;
        case WME_AC_BE:
        default:
            pPhyParam = &ic->phyParamForAC[WME_AC_BE][mode];
            pBssPhyParam = &ic->bssPhyParamForAC[WME_AC_BE][mode];
            break;
        }

        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_acm = pPhyParam->acm;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_aifsn = pPhyParam->aifsn;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmin = pPhyParam->logcwmin;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmax = pPhyParam->logcwmax;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_txopLimit = pPhyParam->txopLimit;
        } else {
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_acm = pBssPhyParam->acm;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_aifsn = pBssPhyParam->aifsn;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmin = pBssPhyParam->logcwmin;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmax = pBssPhyParam->logcwmax;
            wme->wme_wmeChanParams.cap_wmeParams[i].wmep_txopLimit = pBssPhyParam->txopLimit;
        }
        wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_acm = pBssPhyParam->acm;
        wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_aifsn = pBssPhyParam->aifsn;
        wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_logcwmin = pBssPhyParam->logcwmin;
        wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_logcwmax = pBssPhyParam->logcwmax;
        wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_txopLimit = pBssPhyParam->txopLimit;
    }
    /* NB: check ic_bss to avoid NULL deref on initial attach */
    if (vap->iv_bss != NULL) {
        /*
         * Calculate agressive mode switching threshold based
         * on beacon interval.
         */
        wme->wme_hipri_switch_thresh =
            (HIGH_PRI_SWITCH_THRESH * ieee80211_node_get_beacon_interval(vap->iv_bss)) / 100;
        memcpy(&vap->iv_wmestate,&ic->ic_wme,sizeof(ic->ic_wme));

        ieee80211_wme_updateparams_locked(vap);
    }
}

void
ieee80211_wme_initglobalparams(struct ieee80211com *ic)
{
    int i;
    static const struct wme_phyParamType phyParamForAC_BE[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11A            */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11B            */ {          3,                5,                7,                  0,              0 },
        /* IEEE80211_MODE_11G            */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_FH             */ {          3,                5,                7,                  0,              0 },
        /* IEEE80211_MODE_TURBO          */ {          2,                3,                5,                  0,              0 },
        /* IEEE80211_MODE_TURBO          */ {          2,                3,                5,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT20      */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT20      */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT40      */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT40      */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT160    */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT80_80  */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE20     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE20     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE40     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE40     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE80     */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE160    */ {          3,                4,                6,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE80_80  */ {          3,                4,                6,                  0,              0 }};
     static const struct wme_phyParamType phyParamForAC_BK[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11A            */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11B            */ {          7,                5,               10,                  0,              0 },
        /* IEEE80211_MODE_11G            */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_FH             */ {          7,                5,               10,                  0,              0 },
        /* IEEE80211_MODE_TURBO          */ {          7,                3,               10,                  0,              0 },
        /* IEEE80211_MODE_TURBO          */ {          7,                3,               10,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT20      */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT20      */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11NG_HT40      */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11NA_HT40      */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT160    */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AC_VHT80_80  */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE20     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE20     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE40     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXG_HE40     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE80     */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE160    */ {          7,                4,               10,                  0,              0 },
        /* IEEE80211_MODE_11AXA_HE80_80  */ {          7,                4,               10,                  0,              0 }};
     static const struct wme_phyParamType phyParamForAC_VI[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11A            */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11B            */ {          1,                4,               5,                 188,              0 },
        /* IEEE80211_MODE_11G            */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_FH             */ {          1,                4,               5,                 188,              0 },
        /* IEEE80211_MODE_TURBO          */ {          1,                2,               3,                  94,              0 },
        /* IEEE80211_MODE_TURBO          */ {          1,                2,               3,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT20      */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT20      */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT40      */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT40      */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT160    */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT80_80  */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE20     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE20     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE40     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE40     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE80     */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE160    */ {          1,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE80_80  */ {          1,                3,               4,                  94,              0 }};
     static const struct wme_phyParamType phyParamForAC_VO[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11A            */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11B            */ {          1,                3,               4,                 102,              0 },
        /* IEEE80211_MODE_11G            */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_FH             */ {          1,                3,               4,                 102,              0 },
        /* IEEE80211_MODE_TURBO          */ {          1,                2,               2,                  47,              0 },
        /* IEEE80211_MODE_TURBO          */ {          1,                2,               2,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT20      */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT20      */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT40      */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT40      */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT160    */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT80_80  */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE20     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE20     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE40     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE40     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE80     */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE160    */ {          1,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE80_80  */ {          1,                2,               3,                  47,              0 }};
     static const struct wme_phyParamType bssPhyParamForAC_BE[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11A            */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11B            */ {          3,                5,              10,                   0,              0 },
        /* IEEE80211_MODE_11G            */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_FH             */ {          3,                5,              10,                   0,              0 },
        /* IEEE80211_MODE_TURBO          */ {          2,                3,              10,                   0,              0 },
        /* IEEE80211_MODE_TURBO          */ {          2,                3,              10,                   0,              0 },
        /* IEEE80211_MODE_11NA_HT20      */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11NG_HT20      */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11NG_HT40      */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11NA_HT40      */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AC_VHT160    */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AC_VHT80_80  */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXA_HE20     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXG_HE20     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXA_HE40     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXG_HE40     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXA_HE80     */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXA_HE160    */ {          3,                4,              10,                   0,              0 },
        /* IEEE80211_MODE_11AXA_HE80_80  */ {          3,                4,              10,                   0,              0 }};
     static const struct wme_phyParamType bssPhyParamForAC_VI[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11A            */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11B            */ {          2,                4,               5,                 188,              0 },
        /* IEEE80211_MODE_11G            */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_FH             */ {          2,                4,               5,                 188,              0 },
        /* IEEE80211_MODE_TURBO          */ {          2,                2,               3,                  94,              0 },
        /* IEEE80211_MODE_TURBO          */ {          2,                2,               3,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT20      */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT20      */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NG_HT40      */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11NA_HT40      */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT160    */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AC_VHT80_80  */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE20     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE20     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE40     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXG_HE40     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE80     */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE160    */ {          2,                3,               4,                  94,              0 },
        /* IEEE80211_MODE_11AXA_HE80_80  */ {          2,                3,               4,                  94,              0 }};
     static const struct wme_phyParamType bssPhyParamForAC_VO[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11A            */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11B            */ {          2,                3,               4,                 102,              0 },
        /* IEEE80211_MODE_11G            */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_FH             */ {          2,                3,               4,                 102,              0 },
        /* IEEE80211_MODE_TURBO          */ {          1,                2,               2,                  47,              0 },
        /* IEEE80211_MODE_TURBO          */ {          1,                2,               2,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT20      */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT20      */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NG_HT40      */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11NA_HT40      */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT160    */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AC_VHT80_80  */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE20     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE20     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE40PLUS */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE40MINUS*/ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE40PLUS */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE40MINUS*/ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE40     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXG_HE40     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE80     */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE160    */ {          2,                2,               3,                  47,              0 },
        /* IEEE80211_MODE_11AXA_HE80_80  */ {          2,                2,               3,                  47,              0 }};

    for (i = 0; i < WME_NUM_AC; i++) {
        switch (i) {
        case WME_AC_BK:
            qdf_mem_copy(ic->phyParamForAC[WME_AC_BK], phyParamForAC_BK,
                    sizeof(phyParamForAC_BK));
            qdf_mem_copy(ic->bssPhyParamForAC[WME_AC_BK], phyParamForAC_BK,
                    sizeof(phyParamForAC_BK));
            break;
        case WME_AC_VI:
            qdf_mem_copy(ic->phyParamForAC[WME_AC_VI], phyParamForAC_VI,
                    sizeof(phyParamForAC_VI));
            qdf_mem_copy(ic->bssPhyParamForAC[WME_AC_VI], bssPhyParamForAC_VI,
                    sizeof(bssPhyParamForAC_VI));
            break;
        case WME_AC_VO:
            qdf_mem_copy(ic->phyParamForAC[WME_AC_VO], phyParamForAC_VO,
                    sizeof(phyParamForAC_VO));
            qdf_mem_copy(ic->bssPhyParamForAC[WME_AC_VO], bssPhyParamForAC_VO,
                    sizeof(bssPhyParamForAC_VO));
            break;
        case WME_AC_BE:
        default:
            qdf_mem_copy(ic->phyParamForAC[WME_AC_BE], phyParamForAC_BE,
                    sizeof(phyParamForAC_BE));
            qdf_mem_copy(ic->bssPhyParamForAC[WME_AC_BE], bssPhyParamForAC_BE,
                    sizeof(bssPhyParamForAC_BE));
            break;
        }
    }
}

/* Initializes MU EDCA params for the VAP */
void
ieee80211_muedca_initparams(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct muedcaParams *muedca = ic->ic_muedca_defaultParams;
    int iter;

    for(iter = 0; iter < MUEDCA_NUM_AC; iter++) {
        vap->iv_muedcastate.muedca_paramList[iter].muedca_ecwmin =
            muedca[iter].muedca_ecwmin;
        vap->iv_muedcastate.muedca_paramList[iter].muedca_ecwmax =
            muedca[iter].muedca_ecwmax;
        vap->iv_muedcastate.muedca_paramList[iter].muedca_aifsn =
            muedca[iter].muedca_aifsn;
        vap->iv_muedcastate.muedca_paramList[iter].muedca_acm =
            muedca[iter].muedca_acm;
        vap->iv_muedcastate.muedca_paramList[iter].muedca_timer =
            muedca[iter].muedca_timer;
    }

}

/* Initializes MU EDCA params for the IC */
void
ieee80211_muedca_initglobalparams(struct ieee80211com *ic)
{

    static const struct muedcaParams default_params[MUEDCA_NUM_AC] = {
    /* MUEDCA_AC_BE */ {    9,    10,    8,    0,    255},
    /* MUEDCA_AC_BK */ {    9,    10,   15,    0,    255},
    /* MUEDCA_AC_VI */ {    5,     7,    5,    0,    255},
    /* MUEDCA_AC_VO */ {    5,     7,    5,    0,    255}};


    qdf_mem_copy(ic->ic_muedca_defaultParams, default_params,
            sizeof(default_params));

}

static void
ieee80211_vap_iter_check_burst(void *arg, wlan_if_t vap)
{
    u_int8_t *pburstEnabled= (u_int8_t *) arg;
    if (vap->iv_ath_cap & IEEE80211_ATHC_BURST) {
        *pburstEnabled=1;
    }

}

void
ieee80211_wme_amp_overloadparams_locked(struct ieee80211com *ic, struct ieee80211vap *vap)
{
    struct ieee80211_wme_state *wme = &ic->ic_wme;

    wme->wme_chanParams.cap_wmeParams[WME_AC_BE].wmep_txopLimit
        = ic->ic_reg_parm.ampTxopLimit;
    wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE].wmep_txopLimit
        = ic->ic_reg_parm.ampTxopLimit;

    if (wme->wme_update) {
        wme->wme_update(ic, vap, false);
    }
}

/*
 * Update WME parameters for ourself and the BSS.
 */
void
ieee80211_wme_updateparams_locked(struct ieee80211vap *vap)
{
    static const struct { u_int8_t aifsn; u_int8_t logcwmin; u_int8_t logcwmax; u_int16_t txopLimit;}
    phyParam[IEEE80211_MODE_MAX] = {
        /* IEEE80211_MODE_AUTO           */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11A            */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11B            */ {          2,                5,               10,           64 },
        /* IEEE80211_MODE_11G            */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_FH             */ {          2,                5,               10,           64 },
        /* IEEE80211_MODE_TURBO_A        */ {          1,                3,               10,           64 },
        /* IEEE80211_MODE_TURBO_G        */ {          1,                3,               10,           64 },
        /* IEEE80211_MODE_11NA_HT20      */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11NG_HT20      */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11NA_HT40PLUS  */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11NA_HT40MINUS */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11NG_HT40PLUS  */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11NG_HT40MINUS */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11NG_HT40      */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11NA_HT40      */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11AC_VHT20     */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11AC_VHT40PLUS */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11AC_VHT40MINUS*/ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11AC_VHT40     */ {          2,                4,               10,           64 },
        /* IEEE80211_MODE_11AC_VHT80     */ {          2,                4,               10,           64 }};
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_wme_state *wme = &vap->iv_wmestate;
    enum ieee80211_phymode mode;
    int i;

    /* set up the channel access parameters for the physical device */
    for (i = 0; i < WME_NUM_AC; i++) {
        wme->wme_chanParams.cap_wmeParams[i].wmep_acm
            = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_acm;
        wme->wme_chanParams.cap_wmeParams[i].wmep_aifsn
            = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_aifsn;
        wme->wme_chanParams.cap_wmeParams[i].wmep_logcwmin
            = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmin;
        wme->wme_chanParams.cap_wmeParams[i].wmep_logcwmax
            = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmax;
        wme->wme_chanParams.cap_wmeParams[i].wmep_txopLimit
            = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_txopLimit;
        wme->wme_bssChanParams.cap_wmeParams[i].wmep_acm
            = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_acm;
        wme->wme_bssChanParams.cap_wmeParams[i].wmep_aifsn
            = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_aifsn;
        wme->wme_bssChanParams.cap_wmeParams[i].wmep_logcwmin
            = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_logcwmin;
        wme->wme_bssChanParams.cap_wmeParams[i].wmep_logcwmax
            = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_logcwmax;
        wme->wme_bssChanParams.cap_wmeParams[i].wmep_txopLimit
            = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_txopLimit;
    }

    /*
     * Select mode; we can be called early in which case we
     * always use auto mode.  We know we'll be called when
     * entering the RUN state with bsschan setup properly
     * so state will eventually get set correctly
     */
    if (vap->iv_bsschan != IEEE80211_CHAN_ANYC)
        mode = ieee80211_chan2mode(vap->iv_bsschan);
    else
        mode = IEEE80211_MODE_AUTO;

    if ((vap->iv_opmode == IEEE80211_M_HOSTAP &&
        ((wme->wme_flags & WME_F_AGGRMODE) != 0)) ||
        ((vap->iv_opmode == IEEE80211_M_STA || vap->iv_opmode == IEEE80211_M_IBSS) &&
         (vap->iv_bss->ni_flags & IEEE80211_NODE_QOS) == 0) ||
         ieee80211_vap_wme_is_clear(vap)) {
        u_int8_t burstEnabled=0;
        /* check if bursting  enabled on at least one vap */
        wlan_iterate_vap_list(ic,ieee80211_vap_iter_check_burst,(void *) &burstEnabled);
        wme->wme_chanParams.cap_wmeParams[WME_AC_BE].wmep_aifsn = phyParam[mode].aifsn;
        wme->wme_chanParams.cap_wmeParams[WME_AC_BE].wmep_logcwmin = phyParam[mode].logcwmin;
        wme->wme_chanParams.cap_wmeParams[WME_AC_BE].wmep_logcwmax = phyParam[mode].logcwmax;
        wme->wme_chanParams.cap_wmeParams[WME_AC_BE].wmep_txopLimit
            = burstEnabled ? phyParam[mode].txopLimit : 0;
        wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE].wmep_aifsn = phyParam[mode].aifsn;
        wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE].wmep_logcwmin = phyParam[mode].logcwmin;
        wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE].wmep_logcwmax = phyParam[mode].logcwmax;
        wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE].wmep_txopLimit
            = burstEnabled ? phyParam[mode].txopLimit : 0;
    }

    if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
        ic->ic_sta_assoc < 2 &&
        (wme->wme_flags & WME_F_AGGRMODE) != 0) {
        static const u_int8_t logCwMin[IEEE80211_MODE_MAX] = {
            /* IEEE80211_MODE_AUTO           */   3,
            /* IEEE80211_MODE_11A            */   3,
            /* IEEE80211_MODE_11B            */   4,
            /* IEEE80211_MODE_11G            */   3,
            /* IEEE80211_MODE_FH             */   4,
            /* IEEE80211_MODE_TURBO_A        */   3,
            /* IEEE80211_MODE_TURBO_G        */   3,
            /* IEEE80211_MODE_11NA_HT20      */   3,
            /* IEEE80211_MODE_11NG_HT20      */   3,
            /* IEEE80211_MODE_11NA_HT40PLUS  */   3,
            /* IEEE80211_MODE_11NA_HT40MINUS */   3,
            /* IEEE80211_MODE_11NG_HT40PLUS  */   3,
            /* IEEE80211_MODE_11NG_HT40MINUS */   3,
            /* IEEE80211_MODE_11NG_HT40      */   3,
            /* IEEE80211_MODE_11NA_HT40      */   3,
            /* IEEE80211_MODE_11AC_VHT20     */   3,
            /* IEEE80211_MODE_11AC_VHT40PLUS */   3,
            /* IEEE80211_MODE_11AC_VHT40MINUS*/   3,
            /* IEEE80211_MODE_11AC_VHT40     */   3,
            /* IEEE80211_MODE_11AC_VHT80     */   3
        };

        wme->wme_chanParams.cap_wmeParams[WME_AC_BE].wmep_logcwmin
            = wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE].wmep_logcwmin
            = logCwMin[mode];
    }
    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {   /* XXX ibss? */
        u_int8_t    temp_cap_info   = wme->wme_bssChanParams.cap_info;
        u_int8_t    temp_info_count = temp_cap_info & WME_QOSINFO_COUNT;
        /*
         * Arrange for a beacon update and bump the parameter
         * set number so associated stations load the new values.
         */
        if (wme->wme_flags & WME_F_BSSPARAM_UPDATED) {
            temp_info_count++;
            wme->wme_flags &= ~(WME_F_BSSPARAM_UPDATED);
        }

        wme->wme_bssChanParams.cap_info =
            (temp_cap_info & WME_QOSINFO_UAPSD) |
            (temp_info_count & WME_QOSINFO_COUNT);
        ieee80211vap_set_flag(vap, IEEE80211_F_WMEUPDATE);
    }

    /* Override txOpLimit for video stream for now.
     * else WMM failures are observed in 11N testbed
     * Refer to bug #22594 and 31082
     */
    /* NOTE: The above overriding behavior causes new VHT test cases to
     * break. As this is common code retaining behavior for pre-VHT cards
     * modes and overriding the setting only for 11N cards
     */
    if (!(ic->ic_flags_ext & IEEE80211_CEXT_11AC) &&
            (vap->iv_opmode == IEEE80211_M_STA)) {
        if (VAP_NEED_CWMIN_WORKAROUND(vap)) {
             /* For UB9x, override CWmin for video stream for now
             * else WMM failures are observed in 11N testbed
             * Refer to bug #70038 ,#70039 and #70041
             */
            if (wme->wme_chanParams.cap_wmeParams[WME_AC_VI].wmep_logcwmin == 3) {
                wme->wme_chanParams.cap_wmeParams[WME_AC_VI].wmep_logcwmin = 4;
            }
        }
        wme->wme_chanParams.cap_wmeParams[WME_AC_VI].wmep_txopLimit = 0;
    }

#if ATH_SUPPORT_IBSS_WMM
    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        wme->wme_chanParams.cap_wmeParams[WME_AC_BE].wmep_txopLimit = 0;
        wme->wme_chanParams.cap_wmeParams[WME_AC_VI].wmep_txopLimit = 0;
    }
#endif
    /* Copy wme params from vap to ic to avoid inconsistency */
    memcpy(&ic->ic_wme,&vap->iv_wmestate,sizeof(ic->ic_wme));

    if (wme->wme_update) {
        wme->wme_update(ic, vap, false);
    }

#if 0
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
                      "%s: WME params updated, cap_info 0x%x\n", __func__,
                      vap->iv_opmode == IEEE80211_M_STA ?
                      wme->wme_wmeChanParams.cap_info :
                      wme->wme_bssChanParams.cap_info);
#endif
}

/*
 * Update WME parameters for ourself and the BSS.
 */
void
ieee80211_wme_updateinfo_locked(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_wme_state *wme = &ic->ic_wme;

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {   /* XXX ibss? */
        u_int8_t    temp_cap_info   = wme->wme_bssChanParams.cap_info;
        u_int8_t    temp_info_count = temp_cap_info & WME_QOSINFO_COUNT;
        /*
         * Arrange for a beacon update and bump the parameter
         * set number so associated stations load the new values.
         */
        wme->wme_bssChanParams.cap_info =
            (temp_cap_info & WME_QOSINFO_UAPSD) |
            ((temp_info_count + 1) & WME_QOSINFO_COUNT);
        ieee80211vap_set_flag(vap, IEEE80211_F_WMEUPDATE);
    }

    if (wme->wme_update) {
        wme->wme_update(ic, vap, false);
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
                      "%s: WME info updated, cap_info 0x%x\n", __func__,
                      vap->iv_opmode == IEEE80211_M_STA ?
                      wme->wme_wmeChanParams.cap_info :
                      wme->wme_bssChanParams.cap_info);
}

void
ieee80211_wme_updateparams(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_caps & IEEE80211_C_WME) {
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            /*
             * For AP, we need to synchronize with SWBA interrupt.
             * So we need to lock out interrupt.
             */
            OS_EXEC_INTSAFE(ic->ic_osdev, ieee80211_wme_updateparams_locked, vap);
        } else {
            ieee80211_wme_updateparams_locked(vap);
        }
    }
}

void
ieee80211_wme_updateinfo(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_caps & IEEE80211_C_WME) {
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            /*
             * For AP, we need to synchronize with SWBA interrupt.
             * So we need to lock out interrupt.
             */
            OS_EXEC_INTSAFE(ic->ic_osdev, ieee80211_wme_updateinfo_locked, vap);
        } else {
            ieee80211_wme_updateinfo_locked(vap);
        }
    }
}

void
ieee80211_dump_pkt(struct wlan_objmgr_pdev *pdev,
                   const u_int8_t *buf, int len, int rate, int rssi)
{
    const struct ieee80211_frame *wh;
    int type, subtype;
    int i;

    printk("%8p (%lu): \n", buf, (unsigned long) OS_GET_TIMESTAMP());

    wh = (const struct ieee80211_frame *)buf;
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) >> IEEE80211_FC0_SUBTYPE_SHIFT;

    switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
    case IEEE80211_FC1_DIR_NODS:
        printk("NODS %s", ether_sprintf(wh->i_addr2));
        printk("->%s", ether_sprintf(wh->i_addr1));
        printk("(%s)", ether_sprintf(wh->i_addr3));
        break;
    case IEEE80211_FC1_DIR_TODS:
        printk("TODS %s", ether_sprintf(wh->i_addr2));
        printk("->%s", ether_sprintf(wh->i_addr3));
        printk("(%s)", ether_sprintf(wh->i_addr1));
        break;
    case IEEE80211_FC1_DIR_FROMDS:
        printk("FRDS %s", ether_sprintf(wh->i_addr3));
        printk("->%s", ether_sprintf(wh->i_addr1));
        printk("(%s)", ether_sprintf(wh->i_addr2));
        break;
    case IEEE80211_FC1_DIR_DSTODS:
        printk("DSDS %s", ether_sprintf((const u_int8_t *)&wh[1]));
        printk("->%s", ether_sprintf(wh->i_addr3));
        printk("(%s", ether_sprintf(wh->i_addr2));
        printk("->%s)", ether_sprintf(wh->i_addr1));
        break;
    }
    switch (type) {
    case IEEE80211_FC0_TYPE_DATA:
        printk(" data");
        break;
    case IEEE80211_FC0_TYPE_MGT:
        printk(" %s", ieee80211_mgt_subtype_name[subtype]);
        break;
    default:
        printk(" type#%d", type);
        break;
    }
    if (IEEE80211_QOS_HAS_SEQ(wh)) {
        const struct ieee80211_qosframe *qwh =
            (const struct ieee80211_qosframe *)buf;
        printk(" QoS [TID %u%s]", qwh->i_qos[0] & IEEE80211_QOS_TID,
               qwh->i_qos[0] & IEEE80211_QOS_ACKPOLICY ? " ACM" : "");
    }

    if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
        int off;

        off = ieee80211_anyhdrspace(pdev, wh);
        printk(" WEP [IV %.02x %.02x %.02x",
               buf[off+0], buf[off+1], buf[off+2]);
        if (buf[off+IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)
            printk(" %.02x %.02x %.02x",
                   buf[off+4], buf[off+5], buf[off+6]);
        printk(" KID %u]", buf[off+IEEE80211_WEP_IVLEN] >> 6);
    }
    if (rate >= 0)
        printk(" %dM", rate / 1000);
    if (rssi >= 0)
        printk(" +%d", rssi);
    printk("\n");
    if (len > 0) {
        for (i = 0; i < len; i++) {
            if ((i & 1) == 0)
                printk(" ");
            printk("%02x", buf[i]);
        }
        printk("\n");
    }
}

static void
get_20MHz_only(void *arg, struct ieee80211_node *ni)
{
    u_int32_t *num_sta = arg;

    if (!ni->ni_associd)
        return;

    /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on coex rules */
    if ((ni->ni_flags & IEEE80211_NODE_HT || IEEE80211_NODE_USE_HE(ni)) &&
        ((ni->ni_flags & IEEE80211_NODE_40MHZ_INTOLERANT) ||
         (ni->ni_flags & IEEE80211_NODE_REQ_20MHZ))) {
        (*num_sta)++;
    }
}

void
ieee80211_change_cw(struct ieee80211com *ic)
{
    enum ieee80211_cwm_width cw_width = ic->ic_cwm_get_width(ic);
    u_int32_t num_20MHz_only = 0;

    if (ic->ic_flags & IEEE80211_F_COEXT_DISABLE)
        return;

    ieee80211_iterate_node(ic, get_20MHz_only, &num_20MHz_only);

    if (cw_width == IEEE80211_CWM_WIDTH40) {
        if (num_20MHz_only) {
            ic->ic_bss_to20(ic);
        }
    } else if (cw_width == IEEE80211_CWM_WIDTH20) {
        if (num_20MHz_only == 0) {
            ic->ic_bss_to40(ic);
        }
    }
}

void
ath_vap_iter_cac(void *arg, wlan_if_t vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    if (wlan_vdev_mlme_get_state(vap->vdev_obj) != WLAN_VDEV_S_DFS_CAC_WAIT)
        return;

    if (ic->ic_nl_handle)
        return;

    /* Radar Detected */
    if (IEEE80211_IS_CHAN_RADAR(ic->ic_curchan)) {
        wlan_vdev_mlme_sm_deliver_evt(vap->vdev_obj,
                                      WLAN_VDEV_SM_EV_RADAR_DETECTED, 0, NULL);
    } else {
        wlan_vdev_mlme_sm_deliver_evt(vap->vdev_obj,
                                    WLAN_VDEV_SM_EV_DFS_CAC_COMPLETED, 0, NULL);
    }
}

void
ieee80211_dfs_proc_cac(struct ieee80211com *ic)
{
    wlan_iterate_vap_list(ic, ath_vap_iter_cac, NULL);
}

