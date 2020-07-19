/*
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 */

#include <wlan_cmn.h>
#include <qdf_status.h>
#include <reg_services_public_struct.h>
#include <ieee80211_regdmn_dispatcher.h>
#include <osdep.h>
#include "ieee80211_mlme_dfs_dispatcher.h"
#include <ieee80211_var.h>
#include <ieee80211_proto.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include "ieee80211_mlme_priv.h"
#include "ieee80211_bssload.h"
#include "ieee80211_quiet_priv.h"
#include "ieee80211_ucfg.h"
#include "ieee80211_sme_api.h"
#include <wlan_son_pub.h>
#include <wlan_utility.h>
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif
#include <wlan_vdev_mlme.h>
#include "ol_if_athvar.h"
#include "cfg_ucfg_api.h"

#ifndef NUM_MILLISEC_PER_SEC
#define NUM_MILLISEC_PER_SEC 1000
#endif
/*
 *  XXX: Because OID_DOT11_ENUM_BSS_LIST is queried every 30 seconds,
 *       set the interval of Beacon Store to 30.
 *       This is to make sure the AP(GO)'s own scan_entry always exsits in the scan table.
 *       This is a workaround, a better solution is to add reference counter,
 *       to prevent its own scan_entry been flushed out.
 */
#define INTERVAL_STORE_BEACON 30

#define IEEE80211_TSF_LEN       (8)
/*
 *  XXX: Include an intra-module function from ieee80211_input.c.
 *       When we move regdomain code out to separate .h/.c files
 *       this should go to that .h file.
 */

/* Check if the channel is part of PreCAC Donelist of the AP */
#define IS_PRECAC_DONE_FOR_CHAN(_ic,_chan)       \
                mlme_dfs_is_precac_done((_ic)->ic_pdev_obj,  \
                (_chan)->ic_freq,              \
                (_chan)->ic_flags,             \
                (_chan)->ic_flagext,           \
                (_chan)->ic_ieee,              \
                (_chan)->ic_vhtop_ch_freq_seg1,\
                (_chan)->ic_vhtop_ch_freq_seg2)

/* Check if the RootAP has done CAC on the target DFS Channel during CSA */
#define HAS_ROOTAP_POSSIBLY_DONE_CAC(_ic) ((_ic)->ic_has_rootap_done_cac)

/* Check if the Repeater AP has done CAC on the target DFS Channel during CSA.
 * Note that if _c is a subset of _ic->ic_curchan then CAC
 * is already done on _c.
 */
#define HAS_THISAP_DONE_CAC(_ic, _c) \
                ((IS_PRECAC_DONE_FOR_CHAN((_ic), (_c))) || \
                 is_subset_channel_for_cac(_ic, _c, _ic->ic_curchan))

#if ATH_SUPPORT_IBSS_DFS
static u_int8_t
ieee80211_ibss_dfs_element_enable(struct ieee80211vap *vap, struct ieee80211com *ic)
{
    u_int8_t enable_ibssdfs = 1;

    if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
        if (!(vap->iv_bsschan->ic_flagext & IEEE80211_CHAN_DFS)) {
            enable_ibssdfs = 0;
        }
    } else {
        if (!(ic->ic_curchan->ic_flagext & IEEE80211_CHAN_DFS)) {
            enable_ibssdfs = 0;
        }
    }

    return enable_ibssdfs;
}
#endif

#if QCN_IE
static void
ieee80211_flag_beacon_sent(struct ieee80211vap *vap) {
    struct ieee80211com *ic = vap->iv_ic;
    qdf_hrtimer_data_t *bpr_hrtimer = &vap->bpr_timer;

    /* If there is a beacon to be scheduled within the timer window,
     * drop the response and cancel the timer. If timer is not active,
     * qdf_hrtimer_get_remaining will return a negative value, so the timer
     * expiry will be less than beacon timestamp and timer won't be cancelled.
     * If timer expiry is greater than the beacon timestamp, then timer will
     * be cancelled.
     */

    if (qdf_ktime_to_ns(qdf_ktime_add(qdf_hrtimer_get_remaining(bpr_hrtimer), qdf_ktime_get())) >
        qdf_ktime_to_ns(vap->iv_next_beacon_tstamp) + ic->ic_bcn_latency_comp * QDF_NSEC_PER_MSEC) {

        /* Cancel the timer as beacon is sent instead of a broadcast response */
	if (qdf_hrtimer_cancel(bpr_hrtimer)) {
            vap->iv_bpr_timer_cancel_count++;

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                "Cancel timer: %s| %d | Delay: %d | Current time %lld | Next beacon tstamp: %lld | "
                "beacon interval: %d ms | Timer cb: %d | Enqueued: %d\n", \
                __func__, __LINE__, vap->iv_bpr_delay, qdf_ktime_to_ns(qdf_ktime_get()), qdf_ktime_to_ns(vap->iv_next_beacon_tstamp), \
                ic->ic_intval, qdf_hrtimer_callback_running(bpr_hrtimer), qdf_hrtimer_is_queued(bpr_hrtimer));
        }
    }

    /* Calculate the next beacon timestamp */
    vap->iv_next_beacon_tstamp = qdf_ktime_add_ns(qdf_ktime_get(), ic->ic_intval * QDF_NSEC_PER_MSEC);

}
#endif /* QCN_IE */

static uint8_t *ieee80211_add_mbss_ie(struct ieee80211_beacon_offsets *bo, uint8_t *frm,
                                        struct ieee80211_node *ni)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    struct ieee80211vap *tmpvap = NULL;
    u_int8_t *orig_frm = NULL;
    u_int8_t *new = NULL;

    if (!frm || !ni) {
        return NULL;
    }

    vap = ni->ni_vap;
    ic = vap->iv_ic;
    orig_frm = frm;

    /*
     * For a transmitting VAP, if IE already exists and
     * there is atleast one non-transmitting VAP profile,
     * we return right away
     */
    if (!IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
        if (ic->ic_mbss.num_non_transmit_vaps) {
            if (*frm == IEEE80211_ELEMID_MBSSID) {
                bo->bo_mbssid_ie = frm;
                frm += *(frm + 1) + 2;
                return frm;
            }
        }
        else {
            /* No non-transmitting VAPs available, so return */
            return frm;
        }
    }

    /*
     * Add a profile for non-transmitting VAP to IE
     */
    if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
        if (!ieee80211_is_vap_state_stopping(vap)) {
	    if (!qdf_atomic_read(&vap->iv_mbss.iv_added_to_mbss_ie)) {

                frm = ieee80211_add_update_mbssid_ie(frm, ni,
                                                     &ic->ic_mbss.num_non_transmit_vaps,
                                                     IEEE80211_FRAME_TYPE_BEACON);
                if (!frm) {
	            return NULL;
                } else {
		    /* success with adding profile */
                    bo->bo_mbssid_ie = orig_frm;
                    qdf_atomic_set(&vap->iv_mbss.iv_added_to_mbss_ie, 1);
		    return frm;
		}
	    } else {
	        /* VAP profile already part of IE */
	        bo->bo_mbssid_ie = orig_frm;
                frm += *(frm + 1) + 2;
                return frm;
	    }
        } else { /* is_vap_stopping */
	    /*
             * VAP has been stopped, fall through and add
             * other VAPs except this one to IE
             */
            qdf_atomic_set(&vap->iv_mbss.iv_added_to_mbss_ie, 0);
	}
    } /* NON_TRANSMIT_ENABLED */

    /*
     * Below code is run in two cases:
     * 1. If it is a transmitting VAP, IE doesn't exist, and there is
     *    atleast one one-non-transmitting VAP
     * 2. If a non-transmiting VAP has been stopped, other non-transmitting
     *    VAPs are added to IE excluding the VAP in STOPPED state
     */

    ic->ic_mbss.num_non_transmit_vaps = 0;

    TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
        if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(tmpvap) &&
	    (tmpvap->iv_unit != vap->iv_unit) && (tmpvap->iv_is_up)) {
            new = ieee80211_add_update_mbssid_ie(frm, tmpvap->iv_bss,
                                                 &ic->ic_mbss.num_non_transmit_vaps,
                                                 IEEE80211_FRAME_TYPE_BEACON);
	    if (!new)
                return NULL;

            qdf_atomic_set(&tmpvap->iv_mbss.iv_added_to_mbss_ie, 1);
            frm = orig_frm;
        }
    } /* TAILQ_FOREACH */

    if (ic->ic_mbss.num_non_transmit_vaps != 0) {
        frm = new;
        bo->bo_mbssid_ie = orig_frm;
    }

    return frm;
}

/*
 * Delete VAP profile from MBSSID IE
 */
void ieee80211_mbssid_del_profile(struct ieee80211vap *vap)
{
  struct ieee80211com *ic = vap->iv_ic;

  vap->iv_mbss.mbssid_add_del_profile = 1;
  ic->ic_vdev_beacon_template_update(vap);

}

static u_int8_t *
ieee80211_beacon_init(struct ieee80211_node *ni, struct ieee80211_beacon_offsets *bo,
                      u_int8_t *frm)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_rateset *rs = &ni->ni_rates;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
#endif
    int enable_htrates;
    bool add_wpa_ie = true, add_ht_ie = false;
    struct ieee80211_bwnss_map nssmap;
#if UMAC_SUPPORT_WNM
    u_int8_t *fmsie = NULL;
    u_int32_t fms_counter_mask = 0;
    u_int8_t fmsie_len = 0;
#endif /* UMAC_SUPPORT_WNM */
    int num_of_rates = 0, num_of_xrates = 0;
    enum ieee80211_phymode mode;
    struct ieee80211_rateset *rs_op;
    u_int8_t rx_chainmask = ieee80211com_get_rx_chainmask(ic);
#if DBDC_REPEATER_SUPPORT
    struct global_ic_list *ic_list = ic->ic_global_list;
#endif
    struct ieee80211vap *orig_vap;
    struct ieee80211_node *non_transmit_ni = NULL;

    orig_vap = vap = ni->ni_vap;

    if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
        /* We operate on tx vap's beacon buffer */
        vap = ic->ic_mbss.transmit_vap;
        non_transmit_ni = ni;
        ni = vap->iv_bss;
    }

    mode = wlan_get_desired_phymode(vap);
    rs_op = &(vap->iv_op_rates[mode]);

    qdf_mem_zero(&nssmap, sizeof(nssmap));

    KASSERT(vap->iv_bsschan != IEEE80211_CHAN_ANYC, ("no bss chan"));

    frm += IEEE80211_TSF_LEN; /* Skip TSF field */

    *(u_int16_t *)frm = htole16(ieee80211_node_get_beacon_interval(ni));
    frm += 2;

    ieee80211_add_capability(frm, ni);
    bo->bo_caps = (u_int16_t *)frm;
    frm += 2;

    *frm++ = IEEE80211_ELEMID_SSID;

    if (IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) {
        *frm++ = 0;
    } else {
        *frm++ = ni->ni_esslen;
        OS_MEMCPY(frm, ni->ni_essid, ni->ni_esslen);
        frm += ni->ni_esslen;
    }

    bo->bo_rates = frm;
    if (vap->iv_flags_ext2 & IEEE80211_FEXT2_BR_UPDATE) {
        frm = ieee80211_add_rates(frm, rs_op);
    } else{
        frm = ieee80211_add_rates(frm, rs);
    }

    /* XXX better way to check this? */
    if (!IEEE80211_IS_CHAN_FHSS(vap->iv_bsschan)) {
        *frm++ = IEEE80211_ELEMID_DSPARMS;
        *frm++ = 1;
        *frm++ = ieee80211_chan2ieee(ic, vap->iv_bsschan);
    }
    bo->bo_tim = frm;

    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        *frm++ = IEEE80211_ELEMID_IBSSPARMS;
        *frm++ = 2;
        *frm++ = 0; *frm++ = 0;                      /* Review ATIM window */
        bo->bo_tim_len = 0;
    } else {
        struct ieee80211_ath_tim_ie *tie = (struct ieee80211_ath_tim_ie *) frm;

        tie->tim_ie        = IEEE80211_ELEMID_TIM;
        tie->tim_len       = 4;                      /* length */
        tie->tim_count     = 0;                      /* DTIM count */
        tie->tim_period    = vap->vdev_mlme->proto.generic.dtim_period;    /* DTIM period */
        tie->tim_bitctl    = 0;                      /* bitmap control */
        tie->tim_bitmap[0] = 0;                      /* Partial Virtual Bitmap */
        frm               += sizeof(struct ieee80211_ath_tim_ie);
        bo->bo_tim_len     = 1;
    }
    bo->bo_tim_trailer = frm;

    /* cfg80211_TODO: IEEE80211_FEXT_COUNTRYIE
     * ic_country.iso are we populating ?
     * we are building channel list from ic
     * so we should have proper IE generated
     *
     */
    if (IEEE80211_IS_COUNTRYIE_ENABLED(ic) && ieee80211_vap_country_ie_is_set(vap)) {
        frm = ieee80211_add_country(frm, vap);
    }

    // EV 98655 : PWRCNSTR is after channel switch
    bo->bo_chanswitch = frm;

#if ATH_SUPPORT_IBSS_DFS
    if(vap->iv_ibssdfs_state == IEEE80211_IBSSDFS_CHANNEL_SWITCH)
    {
        /* Add the csa ie when we are in CSA mode */
        frm = ieee80211_add_ibss_csa(frm, vap);
        /* decrement TBTT count */
        ((struct ieee80211_ath_channelswitch_ie *)
         (bo->bo_chanswitch))->tbttcount =
                         ic->ic_chanchange_tbtt - vap->iv_chanchange_count;
    }
#endif

    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap)) {
        bo->bo_pwrcnstr = frm;
        *frm++ = IEEE80211_ELEMID_PWRCNSTR;
        *frm++ = 1;
        *frm++ = IEEE80211_PWRCONSTRAINT_VAL(vap);
    } else {
         bo->bo_pwrcnstr = 0;
    }

#if ATH_SUPPORT_IBSS_DFS
    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        if (ieee80211_ibss_dfs_element_enable(vap, ic)) {
            ieee80211_build_ibss_dfs_ie(vap);
            bo->bo_ibssdfs = frm;
            frm = ieee80211_add_ibss_dfs(frm, vap);
        } else {
            bo->bo_ibssdfs = NULL;
            OS_MEMZERO(&vap->iv_ibssdfs_ie_data, sizeof(struct ieee80211_ibssdfs_ie));
        }
    }
#endif /* ATH_SUPPORT_IBSS_DFS */

    frm = ieee80211_quiet_beacon_setup(vap, ic, bo, frm);

    enable_htrates = ieee80211vap_htallowed(vap);

    if (IEEE80211_IS_CHAN_ANYG(vap->iv_bsschan) ||
        IEEE80211_IS_CHAN_11NG(vap->iv_bsschan) ||
        IEEE80211_IS_CHAN_11AXG(vap->iv_bsschan)) {
        bo->bo_erp = frm;
        frm = ieee80211_add_erp(frm, ic);
    } else {
        bo->bo_erp = NULL;
    }

    /*
     * check if os shim has setup RSN IE it self.
     */
    if (!vap->iv_rsn_override) {
        IEEE80211_VAP_LOCK(vap);
        if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length) {
            add_wpa_ie = ieee80211_check_wpaie(vap,
                        vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].ie,
                           vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length);
        }
        /*
         * Check if RSN ie is present in the beacon frame. Traverse the linked list
         * and check for RSN IE.
         * If not present, add it in the driver
         */
        add_wpa_ie=ieee80211_mlme_app_ie_check_wpaie(vap,IEEE80211_FRAME_TYPE_BEACON);
        if (vap->iv_opt_ie.length) {
            add_wpa_ie = ieee80211_check_wpaie(vap, vap->iv_opt_ie.ie,
                                               vap->iv_opt_ie.length);
        }
        IEEE80211_VAP_UNLOCK(vap);
    }

    /* XXX: put IEs in the order of element IDs. */
    bo->bo_xrates = frm;
    if (vap->iv_flags_ext2 & IEEE80211_FEXT2_BR_UPDATE) {
        num_of_rates = rs_op->rs_nrates;
        if (num_of_rates > IEEE80211_RATE_SIZE) {
            num_of_xrates = num_of_rates - IEEE80211_RATE_SIZE;
        }
        if(num_of_xrates > 0){
            frm = ieee80211_add_xrates(frm, rs_op);
        }
    } else {
        frm = ieee80211_add_xrates(frm, rs);
    }

    /* Add QBSS Load IE */
    frm = ieee80211_qbssload_beacon_setup(vap, ni, bo, frm);

    /* Add MBSS IE */
    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        frm = ieee80211_add_mbss_ie(bo, frm,
                                (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(orig_vap)) ?
                                non_transmit_ni: ni);
        if (!frm)
            return NULL;
    }

    /* Add rrm capbabilities, if supported */
    frm = ieee80211_add_rrm_cap_ie(frm, ni);

    /*
     * HT cap. check for vap is done in ieee80211vap_htallowed.
     * remove iv_bsschan check to support multiple channel operation.
     */
    if (ieee80211_vap_wme_is_set(vap) &&
        (IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) ||
         IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
         IEEE80211_IS_CHAN_11N(vap->iv_bsschan)) &&
        enable_htrates) {
        add_ht_ie = true;
    }

    if (add_ht_ie) {
        bo->bo_htcap = frm;
        frm = ieee80211_add_htcap(frm, ni, IEEE80211_FC0_SUBTYPE_BEACON);
    }

#if ATH_SUPPORT_HS20
    if (add_wpa_ie && !vap->iv_osen && !vap->iv_rsn_override) {
#else
    if (add_wpa_ie && !vap->iv_rsn_override) {
#endif

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_RSNA))) {
        frm = wlan_crypto_build_rsnie(vap->vdev_obj, frm, NULL);
        if (!frm)
            return NULL;
    }
#else
    if (RSN_AUTH_IS_RSNA(rsn))
        frm = ieee80211_setup_rsn_ie(vap, frm);
#endif

    }

    if (add_ht_ie) {
        bo->bo_htinfo = frm;
        frm = ieee80211_add_htinfo(frm, ni);
    }

#if ATH_SUPPORT_WAPI
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_WAPI)))
#else
    if (RSN_AUTH_IS_WAI(rsn))
#endif
    {
        frm = ieee80211_setup_wapi_ie(vap, frm);
        if (!frm)
            return NULL;
    }
#endif

    if (add_ht_ie) {
        if (!(ic->ic_flags & IEEE80211_F_COEXT_DISABLE)) {
            bo->bo_obss_scan = frm;
            frm = ieee80211_add_obss_scan(frm, ni);
        } else {
            bo->bo_obss_scan = NULL;
        }
    } else {
        bo->bo_htcap = NULL;
        bo->bo_htinfo = NULL;
        bo->bo_obss_scan = NULL;
    }

    /* Add secondary channel offset element here */
    bo->bo_secchanoffset = frm;

#if UMAC_SUPPORT_WNM
    /* Add WNM specific IEs (like FMS desc...), if supported */
    if (ieee80211_vap_wnm_is_set(vap) && ieee80211_wnm_fms_is_set(vap->wnm)) {
        bo->bo_fms_desc = frm;
        ieee80211_wnm_setup_fmsdesc_ie(ni, 0, &fmsie, &fmsie_len, &fms_counter_mask);
        if (fmsie_len)
            OS_MEMCPY(frm, fmsie, fmsie_len);
        frm += fmsie_len;
        bo->bo_fms_trailer = frm;
        bo->bo_fms_len = (u_int16_t)(frm - bo->bo_fms_desc);
    } else {
        bo->bo_fms_desc = NULL;
        bo->bo_fms_len = 0;
        bo->bo_fms_trailer = NULL;
    }
#endif /* UMAC_SUPPORT_WNM */

    /* Add extended capbabilities, if applicable */
    bo->bo_extcap = frm;
    frm = ieee80211_add_extcap(frm, ni, IEEE80211_FC0_SUBTYPE_BEACON);

    /*
     * VHT capable :
     */
    /* Add VHT cap if device is in 11ac operating mode (or)
    256QAM is enabled in 2.4G */
    if (ieee80211_vap_wme_is_set(vap) &&
        (IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) ||
         IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
         IEEE80211_IS_CHAN_11NG(vap->iv_bsschan)) &&
            ieee80211vap_vhtallowed(vap)) {
        /* Add VHT capabilities IE */
        bo->bo_vhtcap = frm;
        frm = ieee80211_add_vhtcap(frm, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON, NULL, NULL);

        /* Add VHT Operation IE */
        bo->bo_vhtop = frm;
        frm = ieee80211_add_vhtop(frm, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON, NULL);

        /* Add Extended BSS Load IE */
        frm = ieee80211_ext_bssload_beacon_setup(vap, ni, bo, frm);

        /* Add VHT Tx Power Envelope */
        if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap)) {
            bo->bo_vhttxpwr = frm;
            frm = ieee80211_add_vht_txpwr_envlp(frm, ni, ic,
                                        IEEE80211_FC0_SUBTYPE_BEACON,
                                        !IEEE80211_VHT_TXPWR_IS_SUB_ELEMENT);
            bo->bo_vhtchnsw = frm;
        } else {
            bo->bo_vhttxpwr = NULL;
            bo->bo_vhtchnsw = NULL;
        }
    } else {
        bo->bo_vhtcap = NULL;
        bo->bo_vhtop = NULL;
        bo->bo_vhttxpwr = NULL;
        bo->bo_vhtchnsw = NULL;
        bo->bo_ext_bssload = NULL;
    }

    /* Add VHT Vendor specific IE for
        256QAM support in 2.4G Interop */
    if ((ieee80211_vap_wme_is_set(vap) &&
         IEEE80211_IS_CHAN_11NG(vap->iv_bsschan)) &&
            ieee80211vap_vhtallowed(vap) &&
              ieee80211vap_11ng_vht_interopallowed(vap)) {
        /* Add VHT capabilities IE and VHT OP IE in Vendor specific IE*/
        bo->bo_interop_vhtcap = frm;
        frm = ieee80211_add_interop_vhtcap(frm, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON);
    } else {
        bo->bo_interop_vhtcap = NULL;
    }

     /*
     * HE capable :
     */
    if (ieee80211_vap_wme_is_set(vap) &&
        IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) && ieee80211vap_heallowed(vap)) {
        /* Add HE capabilities IE */
        bo->bo_hecap = frm;
        frm = ieee80211_add_hecap(frm, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON);

        /* Add HE Operation IE */
        bo->bo_heop = frm;
        frm = ieee80211_add_heop(frm, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON, NULL);
    } else {
        bo->bo_hecap = NULL;
        bo->bo_heop  = NULL;
    }

    bo->bo_bcca = frm;

#ifdef OBSS_PD
    /* Check if OBSS PD service is enabled and add SRP IE in beacon
     * between BSS Color Change Announcement IE and MU EDCA IE as
     * per section 9.3.3.3 in 11ax draft 3.0 */
    if(ic->ic_he_sr_enable &&
       IEEE80211_IS_CHAN_11AX(ic->ic_curchan) && ieee80211vap_heallowed(vap)) {
        bo->bo_srp_ie = frm;
        frm = ieee80211_add_srp_ie(ic, frm);
    }
#endif

    if(ieee80211_vap_wme_is_set(vap) &&
       ieee80211vap_heallowed(vap) &&
       ieee80211vap_muedca_is_enabled(vap)) {
        /* Add MU EDCA parameter set IE */
        bo->bo_muedca = frm;
        frm = ieee80211_add_muedca_param(frm, &vap->iv_muedcastate);
    } else {
        bo->bo_muedca = NULL;
    }

    if (add_wpa_ie && !vap->iv_rsn_override) {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_WPA))) {
            frm = wlan_crypto_build_wpaie(vap->vdev_obj, frm);
            if (!frm)
                return NULL;
        }
#else
        if (RSN_AUTH_IS_WPA(rsn)) {
            frm = ieee80211_setup_wpa_ie(vap, frm);
        }
#endif
    }

    if (ieee80211_vap_wme_is_set(vap) &&
        (vap->iv_opmode == IEEE80211_M_HOSTAP ||
#if ATH_SUPPORT_IBSS_WMM
         vap->iv_opmode == IEEE80211_M_IBSS ||
#endif
         vap->iv_opmode == IEEE80211_M_BTAMP)) {

        bo->bo_wme = frm;
        frm = ieee80211_add_wme_param(frm, &vap->iv_wmestate, IEEE80211_VAP_IS_UAPSD_ENABLED(vap));
        ieee80211vap_clear_flag(vap, IEEE80211_F_WMEUPDATE);
    } else {
        bo->bo_wme = NULL;
    }

    if ((IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) ||
         IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
         IEEE80211_IS_CHAN_11N(vap->iv_bsschan)) &&
            IEEE80211_IS_HTVIE_ENABLED(ic) && enable_htrates) {
        frm = ieee80211_add_htcap_vendor_specific(frm, ni, IEEE80211_FC0_SUBTYPE_BEACON);

        bo->bo_htinfo_vendor_specific = frm;
        frm = ieee80211_add_htinfo_vendor_specific(frm, ni);
    } else {
        bo->bo_htinfo_vendor_specific = NULL;
    }

    bo->bo_ath_caps = frm;
    if (vap->iv_ena_vendor_ie == 1) {
	    if (vap->iv_bss && vap->iv_bss->ni_ath_flags) {
		    frm = ieee80211_add_athAdvCap(frm, vap->iv_bss->ni_ath_flags,
				    vap->iv_bss->ni_ath_defkeyindex);
	    } else {
		    frm = ieee80211_add_athAdvCap(frm, 0, IEEE80211_INVAL_DEFKEY);
	    }
        vap->iv_update_vendor_ie = 0;
    }

#if QCN_IE
    /* Add QCN IE for the feature set */
    bo->bo_qcn_ie = frm;
    frm = ieee80211_add_qcn_info_ie(frm, vap, &bo->bo_qcn_ie_len, NULL);
    frm = ieee80211_add_qcn_ie_mac_phy_params(frm, vap);
#endif
#if QCN_ESP_IE
    /* Add ESP IE for the feature set */
    if(ic->ic_esp_periodicity){
        bo->bo_esp_ie = frm;
        frm = ieee80211_add_esp_info_ie(frm, ic, &bo->bo_esp_ie_len);
    }
#endif

    if(IEEE80211_IS_CSH_OPT_APRIORI_NEXT_CHANNEL_ENABLED(ic)
            && IEEE80211_IS_CHAN_DFS(ic->ic_curchan) && ic->ic_tx_next_ch)
    {
        bo->bo_apriori_next_channel = frm;
        frm = ieee80211_add_next_channel(frm, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON);
    } else {
        bo->bo_apriori_next_channel = NULL;
    }

    /* Add software and hardware version ie to beacon */
    bo->bo_software_version_ie = frm;
    frm = ieee80211_add_sw_version_ie(frm, ic);

    /* Insert ieee80211_ie_ath_extcap IE to beacon */
    if (ic->ic_ath_extcap)
        frm = ieee80211_add_athextcap(frm,
                ic->ic_ath_extcap, ic->ic_weptkipaggr_rxdelim);

    /* Send prop IE if EXT NSS is not supported */
    if (!(vap->iv_ext_nss_support) &&
            !(ic->ic_disable_bcn_bwnss_map) &&
            !(ic->ic_disable_bwnss_adv) &&
            !ieee80211_get_bw_nss_mapping(vap, &nssmap, rx_chainmask)) {
        bo->bo_bwnss_map = frm;
        frm = ieee80211_add_bw_nss_maping(frm, &nssmap);
    } else {
        bo->bo_bwnss_map = NULL;
    }

    if (vap->ap_chan_rpt_enable) {
        bo->bo_ap_chan_rpt = frm;
        frm = ieee80211_add_ap_chan_rpt_ie (frm, vap);
    } else {
        bo->bo_ap_chan_rpt = NULL;
    }

    if (vap->rnr_enable) {
        bo->bo_rnr = frm;
        frm = ieee80211_add_rnr_ie(frm, vap, vap->iv_bss->ni_essid, vap->iv_bss->ni_esslen);
    } else {
        bo->bo_rnr = NULL;
    }

    if (ieee80211_vap_mbo_check(vap) || ieee80211_vap_oce_check(vap)) {
        bo->bo_mbo_cap = frm;
        frm = ieee80211_setup_mbo_ie(IEEE80211_FC0_SUBTYPE_BEACON, vap, frm, ni);
    } else {
        bo->bo_mbo_cap = NULL;
    }

    bo->bo_xr = frm;

    if (IEEE80211_VAP_IS_WDS_ENABLED(vap) &&
            !son_vdev_map_capability_get(vap->vdev_obj, SON_MAP_CAPABILITY)) {
        u_int16_t whcCaps = QCA_OUI_WHC_AP_INFO_CAP_WDS;

        bo->bo_whc_apinfo = frm;
        /* SON mode requires WDS as a prereq */
        if (son_vdev_feat_capablity(vap->vdev_obj, SON_CAP_GET, WLAN_VDEV_F_SON)) {
            whcCaps |= QCA_OUI_WHC_AP_INFO_CAP_SON;
        }
        frm = son_add_ap_info_ie(frm, whcCaps, vap->vdev_obj, &bo->bo_whc_apinfo_len);
    }

#if DBDC_REPEATER_SUPPORT
    if (ic_list->same_ssid_support) {
        /* Add the Extender IE */
        bo->bo_extender_ie = frm;
        frm = ieee80211_add_extender_ie(vap, IEEE80211_FRAME_TYPE_BEACON, frm);
    }
#endif

    bo->bo_appie_buf = frm;
    bo->bo_appie_buf_len = 0;

    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_opt_ie.length) {
        OS_MEMCPY(frm, vap->iv_opt_ie.ie,
                  vap->iv_opt_ie.length);
        frm += vap->iv_opt_ie.length;
    }

    if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length > 0) {
        OS_MEMCPY(frm,vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].ie,
                  vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length);
        frm += vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length;
        bo->bo_appie_buf_len = vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length;
    } else {
        bo->bo_appie_buf_len = 0;
    }

    /* Add the Application IE's */
    frm = ieee80211_mlme_app_ie_append(vap, IEEE80211_FRAME_TYPE_BEACON, frm);
    bo->bo_appie_buf_len += vap->iv_app_ie_list[IEEE80211_FRAME_TYPE_BEACON].total_ie_len;
    IEEE80211_VAP_UNLOCK(vap);

    bo->bo_tim_trailerlen = frm - bo->bo_tim_trailer;
    bo->bo_chanswitch_trailerlen = frm - bo->bo_chanswitch;
    bo->bo_vhtchnsw_trailerlen = frm - bo->bo_vhtchnsw;
    bo->bo_secchanoffset_trailerlen = frm - bo->bo_secchanoffset;
    bo->bo_bcca_trailerlen          = frm - bo->bo_bcca;
#if ATH_SUPPORT_IBSS_DFS
    if (vap->iv_opmode == IEEE80211_M_IBSS &&
        (bo->bo_ibssdfs != NULL)) {
        struct ieee80211_ibssdfs_ie * ibss_dfs_ie = (struct ieee80211_ibssdfs_ie *)bo->bo_ibssdfs;
        bo->bo_ibssdfs_trailerlen = frm - (bo->bo_ibssdfs + 2 + ibss_dfs_ie->len);
    } else {
        bo->bo_ibssdfs_trailerlen = 0;
    }
#endif /* ATH_SUPPORT_IBSS_DFS */
#if UMAC_SUPPORT_WNM
    bo->bo_fms_trailerlen = frm - bo->bo_fms_trailer;
#endif /* UMAC_SUPPORT_WNM */
    return frm;
}

/*
 * Make a copy of the Beacon Frame store for this VAP. NOTE: this copy is not the
 * most recent and is only updated when certain information (listed below) changes.
 *
 * The frame includes the beacon frame header and all the IEs, does not include the 802.11
 * MAC header. Beacon frame format is defined in ISO/IEC 8802-11. The beacon frame
 * should be the up-to-date one used by the driver except that real-time parameters or
 * information elements that vary with data frame flow control or client association status,
 * such as timestamp, radio parameters, TIM, ERP and HT information elements do not
 * need to be accurate.
 *
 */
static void
store_beacon_frame(struct ieee80211vap *vap, u_int8_t *wh, int frame_len)
{

    if (ieee80211_vap_copy_beacon_is_clear(vap)) {
        ASSERT(0);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                "ieee80211_vap_copy_beacon_is_clear is true\n");
        return;
    }

    if (vap->iv_beacon_copy_buf == NULL) {
        /* The beacon copy buffer is not allocated yet. */

        vap->iv_beacon_copy_buf = OS_MALLOC(vap->iv_ic->ic_osdev, IEEE80211_RTS_MAX, GFP_KERNEL);
        if (vap->iv_beacon_copy_buf == NULL) {
            /* Unable to allocate the memory */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Unable to alloc beacon copy buf. Size=%d\n",
                              __func__, IEEE80211_RTS_MAX);
            return;
        }
    }

    ASSERT(frame_len <= IEEE80211_RTS_MAX);
    OS_MEMCPY(vap->iv_beacon_copy_buf, wh, frame_len);
    vap->iv_beacon_copy_len = frame_len;
#if UMAC_SUPPORT_P2P
/*
 *  XXX: When P2P connect, the wireless connection icon will be changed to Red-X,
 *       while the connection is OK.
 *       It is because of the query of OID_DOT11_ENUM_BSS_LIST.
 *       By putting AP(GO)'s own beacon information into the scan table,
 *       that problem can be solved.
 */
    ieee80211_scan_table_update(vap,
                                (struct ieee80211_frame*)wh,
                                frame_len,
                                IEEE80211_FC0_SUBTYPE_BEACON,
                                0,
                                ieee80211_get_current_channel(vap->iv_ic));
#endif
}

/*
 * Allocate a beacon frame and fillin the appropriate bits.
 */
wbuf_t
ieee80211_beacon_alloc(struct ieee80211_node *ni,
                       struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    u_int8_t *frm;

    /* 
     * For non-tx MBSS VAP, we reinitialize the beacon buffer of tx VAP
     * and return.
     */
    if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
       struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(ic->ic_mbss.transmit_vap);

       vap->iv_mbss.mbssid_add_del_profile = 1;
       ic->ic_vdev_beacon_template_update(vap);
       return avn->av_wbuf;
    }

    if (ic && ic->ic_osdev) {
        wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_BEACON, MAX_TX_RX_PACKET_SIZE);
    } else {
        return NULL;
    }

    if (wbuf == NULL)
        return NULL;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
        IEEE80211_FC0_SUBTYPE_BEACON;
    wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
    *(u_int16_t *)wh->i_dur = 0;
    if(ic->ic_softap_enable){
        IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_myaddr);
    }
    IEEE80211_ADDR_COPY(wh->i_addr1, IEEE80211_GET_BCAST_ADDR(ic));
    IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
    IEEE80211_ADDR_COPY(wh->i_addr3, ni->ni_bssid);
    *(u_int16_t *)wh->i_seq = 0;

    frm = (u_int8_t *)&wh[1];

    OS_MEMZERO(frm, IEEE80211_TSF_LEN); /* Clear TSF field */
    frm = ieee80211_beacon_init(ni, bo, frm);
    if (!frm) {
        wbuf_release(ic->ic_osdev, wbuf);
        return NULL;
    }

    if (ieee80211_vap_copy_beacon_is_set(vap)) {
        store_beacon_frame(vap, (u_int8_t *)wh, (frm - (u_int8_t *)wh));
    }

    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *) wbuf_header(wbuf)));

    ni = ieee80211_try_ref_node(ni);
    if (!ni) {
        wbuf_release(ic->ic_osdev, wbuf);
        return NULL;
    } else {
        wlan_wbuf_set_peer_node(wbuf, ni);
    }

    IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_MLME, vap->iv_myaddr,
                       "%s \n", __func__);
    return wbuf;
}


/*
 * Suspend or Resume the transmission of beacon for this SoftAP VAP.
 * @param vap           : vap pointer.
 * @param en_suspend    : boolean flag to enable or disable suspension.
 * @ returns 0 if success, others if failed.
 */
int
ieee80211_mlme_set_beacon_suspend_state(
    struct ieee80211vap *vap,
    bool en_suspend)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;
    struct ieee80211com *ic = vap->iv_ic;
    int ret = 0;

    ASSERT(mlme_priv != NULL);
    if (en_suspend) {
        mlme_priv->im_beacon_tx_suspend++;
        /* Send beacon control command to disable beacon tx */
        if (ic->ic_beacon_offload_control) {
            ret = ic->ic_beacon_offload_control(vap, IEEE80211_BCN_OFFLD_TX_DISABLE);
        }
    }
    else {
        mlme_priv->im_beacon_tx_suspend--;
        /* Send beacon control command to enable beacon tx */
        if (ic->ic_beacon_offload_control) {
            ret = ic->ic_beacon_offload_control(vap, IEEE80211_BCN_OFFLD_TX_ENABLE);
        }
    }

    if (ret) {
        qdf_print("Failed to send beacon offload control message");
    }

    return ret;
}

bool
ieee80211_mlme_beacon_suspend_state(
    struct ieee80211vap *vap)
{
    struct ieee80211_mlme_priv    *mlme_priv = vap->iv_mlme_priv;

    ASSERT(mlme_priv != NULL);
    return (mlme_priv->im_beacon_tx_suspend != 0);
}
#if DYNAMIC_BEACON_SUPPORT
void ieee80211_mlme_set_dynamic_beacon_suspend(struct ieee80211vap *vap, bool suspend_beacon)
{
    if (vap->iv_dbeacon_runtime != suspend_beacon) {
        wlan_deauth_all_stas(vap); /* dissociating all associated stations */
    }
    if (!suspend_beacon ) { /* DFS channels */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "Resume beacon \n");
        qdf_spin_lock_bh(&vap->iv_dbeacon_lock);
        OS_CANCEL_TIMER(&vap->iv_dbeacon_suspend_beacon);
        if (ieee80211_mlme_beacon_suspend_state(vap)) {
            ieee80211_mlme_set_beacon_suspend_state(vap, false);
        }
        vap->iv_dbeacon_runtime = suspend_beacon;
        qdf_spin_unlock_bh(&vap->iv_dbeacon_lock);
    } else { /* non DFS channels */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "Suspend beacon \n");
        qdf_spin_lock_bh(&vap->iv_dbeacon_lock);
        if (!ieee80211_mlme_beacon_suspend_state(vap)) {
            ieee80211_mlme_set_beacon_suspend_state(vap, true);
        }
        vap->iv_dbeacon_runtime = suspend_beacon;
        qdf_spin_unlock_bh(&vap->iv_dbeacon_lock);
    }
}
EXPORT_SYMBOL(ieee80211_mlme_set_dynamic_beacon_suspend);
#endif
int
ieee80211_bcn_prb_template_update(struct ieee80211_node *ni,
                                   struct ieee80211_bcn_prb_info *templ)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;

    /* If Beacon Tx is suspended, then don't send this beacon */
    if (ieee80211_mlme_beacon_suspend_state(vap)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
            "%s: skip Tx beacon during to suspend.\n", __func__);
        return -1;
    }

    /* if vap is paused do not send any beacons */
    if (ieee80211_vap_is_paused(vap)) {
        return -1;
    }
    /* Update capabilities bit */
    if (vap->iv_opmode == IEEE80211_M_IBSS)
        templ->caps = IEEE80211_CAPINFO_IBSS;
    else
        templ->caps = IEEE80211_CAPINFO_ESS;
    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
        templ->caps |= IEEE80211_CAPINFO_PRIVACY;
    if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
        IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan))
        templ->caps |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
    if (ic->ic_flags & IEEE80211_F_SHSLOT)
        templ->caps |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap))
        templ->caps |= IEEE80211_CAPINFO_SPECTRUM_MGMT;

    if (IEEE80211_VAP_IS_PUREB_ENABLED(vap)){
        templ->caps &= ~IEEE80211_CAPINFO_SHORT_SLOTTIME;
    }
    /* set rrm capbabilities, if supported */
    if (ieee80211_vap_rrm_is_set(vap)) {
        templ->caps |= IEEE80211_CAPINFO_RADIOMEAS;
    }

    /* Update ERP Info */
    if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
        vap->iv_opmode == IEEE80211_M_BTAMP) { /* No IBSS Support */
        if (ieee80211_vap_erpupdate_is_set(vap)) {
            if (ic->ic_nonerpsta != 0 )
                templ->erp |= IEEE80211_ERP_NON_ERP_PRESENT;
            if (ic->ic_flags & IEEE80211_F_USEPROT)
                templ->erp |= IEEE80211_ERP_USE_PROTECTION;
            if (ic->ic_flags & IEEE80211_F_USEBARKER)
                templ->erp |= IEEE80211_ERP_LONG_PREAMBLE;
        }
    }

    /* TBD:
     * There are some more elements to be passed from the host
     * HT caps, HT info, Advanced caps, IBSS DFS, WPA, RSN, RRM
     * APP IEs
     */
    return 0;
}

static void ieee80211_disconnect_sta_vap(struct ieee80211com *ic,
        struct ieee80211vap *stavap)
{
    struct wlan_objmgr_pdev *pdev = NULL;
    QDF_STATUS status;
    int val=0;

    wlan_mlme_sm_get_curstate(stavap, IEEE80211_PARAM_CONNECTION_SM_STATE,
            &val);
    if(val == WLAN_ASSOC_STATE_TXCHANSWITCH) {
        /* If Channel change is initiated by STAVAP then do not indicate mlme
         * sta radar detect (in other words do not disconnect the STA) since
         * STA VAP is trying to come up in a different channel and is doing the
         * Channel Switch, STA is yet to do CAC+ send probe req+ AUTH to the
         * Root AP.
         */
    } else {
        if (!(ic->ic_repeater_move.state == REPEATER_MOVE_START)) {
            pdev = ic->ic_pdev_obj;
            status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_OSIF_SCAN_ID);
            if (QDF_IS_STATUS_ERROR(status)) {
                scan_info("unable to get reference");
            } else {
                ucfg_scan_flush_results(pdev, NULL);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_OSIF_SCAN_ID);
            }

            /* Disconnect the main sta vap form RootAP, so that in Dependent mode
             * AP vap(s) automatically goes down. Main sta vap scans and connects
             * to the RootAP in the new channel.
             */
            ieee80211_indicate_sta_radar_detect(stavap->iv_bss);
        }
    }
}

void ieee80211_send_chanswitch_complete_event(
        struct ieee80211com *ic)
{
    struct ieee80211vap *stavap;

    STA_VAP_DOWNUP_LOCK(ic);
    stavap = ic->ic_sta_vap;
    if(stavap) {
        /* Only for main STA send the chanswitch complete event */
        int val=0;

        wlan_mlme_sm_get_curstate(stavap, IEEE80211_PARAM_CONNECTION_SM_STATE,
                &val);
        if(val == WLAN_ASSOC_STATE_TXCHANSWITCH) {
            IEEE80211_DELIVER_EVENT_MLME_TXCHANSWITCH_COMPLETE(stavap,
                    IEEE80211_STATUS_SUCCESS);
        }
    }
    STA_VAP_DOWNUP_UNLOCK(ic);
}

static void ieee80211_beacon_change_channel(
        struct ieee80211vap *vap,
        struct ieee80211_ath_channel *c)
{
    struct ieee80211com *ic = vap->iv_ic;
    enum ieee80211_cwm_width ic_cw_width;
    enum ieee80211_cwm_width ic_cw_width_prev = ic->ic_cwm_get_width(ic);
    struct ieee80211vap *tmp_vap = NULL;

    ic_cw_width = ic->ic_cwm_get_width(ic);

    TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
        if(tmp_vap->iv_opmode == IEEE80211_M_HOSTAP ||
                tmp_vap->iv_opmode == IEEE80211_M_STA  ||
                tmp_vap->iv_opmode == IEEE80211_M_MONITOR) {

            tmp_vap->iv_bsschan = c;
            tmp_vap->iv_des_chan[tmp_vap->iv_des_mode] = c;
            tmp_vap->iv_cur_mode = ieee80211_chan2mode(c);
            tmp_vap->iv_chanchange_count = 0;
            ieee80211vap_clear_flag(tmp_vap, IEEE80211_F_CHANSWITCH);
            tmp_vap->iv_remove_csa_ie = true;
            tmp_vap->iv_no_restart = false;

            if ((tmp_vap->iv_opmode == IEEE80211_M_STA) &&
                    !(IEEE80211_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(c)))
                ieee80211_node_set_chan(tmp_vap->iv_bss);

            /*
             * If multiple vdev restart is supported,channel_switch_set_channel
             * should not be called in CSA case. Check ic_csa_num_vdevs before
             * calling channel_switch_set_channel.
             *
             *
             * If in Rep Independent mode we get a channel change from
             * CAP, set channel for sta vap's, only if it is not in DFS chan.
             * This prevents a case in which STA vap assoc state machine
             * was going into bad state. The reason being, when sta vap moves
             * to DFS channel it sends disconnet notifiction to supplicant but
             * we were doing vap reset before a proper reply hence causing bad
             * state.
             */
            if (!((IEEE80211_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(c)) &&
                   !ieee80211_is_instant_chan_change_possible(vap, c) &&
                   tmp_vap->iv_opmode == IEEE80211_M_STA) &&
                   (wlan_vdev_is_up(tmp_vap->vdev_obj) == QDF_STATUS_SUCCESS)) {

                if (tmp_vap->iv_opmode == IEEE80211_M_STA) {
                      wlan_pdev_mlme_vdev_sm_seamless_chan_change(ic->ic_pdev_obj,
                                                         tmp_vap->vdev_obj, c);
                }
                else {
                     wlan_vdev_mlme_sm_deliver_evt(tmp_vap->vdev_obj,
                                        WLAN_VDEV_SM_EV_CSA_COMPLETE, 0, NULL);
                }

                /* In case of MBSSID, channel_switch_set_channel() is called in
                 * the for loop for all the vaps. Before receiving the start
                 * response for a vap(say vap-A) ieee80211_beacon_update() for
                 * vap-B is called and vap-B reinits the beacon and sets
                 * channel_switch_state = 0. Hence the vap-A skips the CAC on
                 * reception of vdev_start response from FW in
                 * ieee80211_dfs_cac_start(). Once vap-A skips the CAC it enters
                 * RUN state and therefore  all other vaps skip the CAC.
                 *
                 * To fix the above problem, after channel change clear
                 * vap_active flag for all the vaps and do not send beacon if
                 * vap_active flag is cleared.
                 */
            }

            if ((tmp_vap->iv_opmode == IEEE80211_M_HOSTAP) &&
                    vap->iv_unit != tmp_vap->iv_unit)
                tmp_vap->channel_change_done = 1;

            /* Channel width changed.
             * Update BSS node with the new channel width.
             */
            if((ic_cw_width_prev != ic_cw_width) && (tmp_vap->iv_bss != NULL) &&
               (!wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                            WLAN_PDEV_F_MULTIVDEV_RESTART) ||
                              (ic->ic_csa_num_vdevs == 0))) {
                tmp_vap->iv_bss->ni_chwidth = ic->ic_cwm_get_width(ic);
                ic->ic_chwidth_change(tmp_vap->iv_bss);
            }
        }
    }
}

/* **** Channel Switch Algorithm **** *
 * New channel Non-DFS:-
 * 1)Do instant channel change for all vaps.
 *
 * New channel DFS:-
 * 1)Bring down the main STA VAP if present. In dependent mode the STA
 *   brings down the AP VAP(s) and, when re-connected, it brings up AP
 *   VAP(s).
 * 2)If main STA is not present or in independent mode, then
 *   do instant channel change for all VAPs. The channel change
 *   takes care of the CAC automatically.
 *
 * Two types of channel change:-
 * 1)Channel change driven by CSA from root AP
 * 2)Channel change by (A) RADAR or (B) user channel change
 */

/* ieee80211_is_instant_chan_change_possible() - Check Instant Channel
 * Change to Target Channel is possible if :
 *  A) The Target channel is a non-DFS channel.
 *            or
 *  B) The Target channel is a DFS channel and
 *     B.1.The Root AP  is going to come up in the DFS channel immediately (as a result of PreCAC or Bandwidth Reduction).
 *     and
 *     B.2. This AP has already done CAC.
 *          B.2.1:Because the target channel is subset of current channel (bandwidth reduction).
 *          B.2.2:Because the pre-CAC is complete on the target channel.
 */
bool ieee80211_is_instant_chan_change_possible(struct ieee80211vap *vap,
                                               struct ieee80211_ath_channel *c)
{
    bool is_possible;
    struct ieee80211com *ic = vap->iv_ic;
    bool chan_nondfs = !IEEE80211_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(c);
    bool rootap_done_cac = HAS_ROOTAP_POSSIBLY_DONE_CAC(ic);
    bool thisap_done_cac = HAS_THISAP_DONE_CAC(ic, c);

    if (ic->ic_sta_vap && wlan_is_connected(ic->ic_sta_vap))
        is_possible = chan_nondfs || (rootap_done_cac && thisap_done_cac);
    else
        is_possible = chan_nondfs || thisap_done_cac;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,"%s: Max Chan Switch time=%u chan_nondfs=%u, rootap_done_cac=%u, thisap_done_cac=%u \n", __func__,
            ic->ic_mcst_of_rootap,
            chan_nondfs,
            rootap_done_cac,
            thisap_done_cac
            );
    return is_possible;
}

static void inline ieee80211_chan_switch_to_new_chan(
        struct ieee80211vap *vap,
        struct ieee80211_ath_channel *c)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap *stavap = NULL;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,"%s: Prev Chan=%u New Chan=%u mainsta=%pk enh_ind=%u\n",
            __func__,ic->ic_curchan->ic_ieee,
            c->ic_ieee,ic->ic_sta_vap,
            ieee80211_ic_enh_ind_rpt_is_set(ic));
    if (ieee80211_is_instant_chan_change_possible(vap, c)) {
        /* If instant channel change is possible, do instant channel change in both
         * dependent and Idependent mode
         */
        ieee80211_beacon_change_channel(vap, c);
    } else {
        STA_VAP_DOWNUP_LOCK(ic);
        stavap = ic->ic_sta_vap;
        if(stavap)
            ieee80211_disconnect_sta_vap(ic, stavap);
        if(ieee80211_ic_enh_ind_rpt_is_set(ic) || !stavap ||
           (ic->ic_repeater_move.state == REPEATER_MOVE_START)) {
            STA_VAP_DOWNUP_UNLOCK(ic);
            ieee80211_beacon_change_channel(vap, c);
        } else {
            STA_VAP_DOWNUP_UNLOCK(ic);
        }
    }

    ieee80211_send_chanswitch_complete_event(ic);
}

/* This function adds the Maximum Channel Switch Time IE in the beacon
 *
 * "This element is optionally present in Beacon and Probe Response frames
 * when a Channel Switch Announcement or Extended Channel Switch Announcement
 * element is also present." -- Quote from ieee80211 standard
 *
 * "The Max Channel Switch Time element indicates the time delta between
 * the time the last beacon is transmitted by the AP in the current channel
 * and the expected time of the first beacon transmitted by the AP
 * in the new channel". -- Quote from ieee80211 standard
 *
 *@frm: pointer to the beacon where the IE should be written
 *@max_time: The time delta between  the last beacon TXed in the current
 *           channel and the first beacon in the new channel. In TUs.
 */
static inline void ieee80211_add_max_chan_switch_time_ie(
        uint8_t *frm,
        uint32_t max_time)
{
    struct ieee80211_max_chan_switch_time_ie *max_chan_switch_time_ie;
    uint8_t i;

    max_chan_switch_time_ie = (struct ieee80211_max_chan_switch_time_ie *) frm;
    max_chan_switch_time_ie->elem_id = IEEE80211_ELEMID_EXTN;
    max_chan_switch_time_ie->elem_len = MAX_CHAN_SWITCH_TIME_IE_LEN;
    max_chan_switch_time_ie->elem_id_ext = IEEE80211_ELEMID_EXT_MAX_CHAN_SWITCH_TIME;

    /* Pack the max_time in 3 octets/bytes. Little endian format */
    for(i = 0; i < SIZE_OF_MAX_TIME_INT; i++) {
        max_chan_switch_time_ie->switch_time[i] = (max_time & ONE_BYTE_MASK);
        max_time = (max_time >> BITS_IN_A_BYTE);
    }
}

void ieee80211_add_max_chan_switch_time(struct ieee80211vap *vap, uint8_t *frm)
{
    struct ieee80211com *ic = vap->iv_ic;
    int cac_timeout = 0;
    uint32_t max_switch_time_in_ms = 0;
    uint32_t max_switch_time_in_tu = 0;
    uint32_t cac_in_ms = 0;
    uint32_t beacon_interval_in_ms = (uint32_t)ieee80211_vap_get_beacon_interval(vap);
    int dfs_region;
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;

    ieee80211_regdmn_get_dfs_region(pdev, (enum dfs_reg *)&dfs_region);
    if(IEEE80211_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(ic->ic_chanchange_channel)) {
        if((dfs_region == DFS_ETSI_DOMAIN &&
            mlme_dfs_is_precac_done(pdev,ic->ic_chanchange_channel->ic_freq,
                ic->ic_chanchange_channel->ic_flags,
                ic->ic_chanchange_channel->ic_flagext,
                ic->ic_chanchange_channel->ic_ieee,
                ic->ic_chanchange_channel->ic_vhtop_ch_freq_seg1,
                ic->ic_chanchange_channel->ic_vhtop_ch_freq_seg2)) ||
                is_subset_channel_for_cac(ic, ic->ic_chanchange_channel, ic->ic_curchan)) {
                    cac_in_ms = 0;
                } else {
                    cac_timeout = ieee80211_dfs_get_cac_timeout(ic, ic->ic_chanchange_channel);
                    cac_in_ms = cac_timeout * 1000;
                }
    }

    max_switch_time_in_ms = cac_in_ms + beacon_interval_in_ms;
    if(ic->ic_mcst_of_rootap > max_switch_time_in_ms)
    {
        max_switch_time_in_ms = ic->ic_mcst_of_rootap;
    }
    max_switch_time_in_tu = IEEE80211_MS_TO_TU(max_switch_time_in_ms);

    /* Add Maximum Channel Switch Time IE */
    ieee80211_add_max_chan_switch_time_ie(frm, max_switch_time_in_tu);
}

static void inline ieee80211_add_channel_switch_ie(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_extendedchannelswitch_ie *ecsa_ie = NULL;
    struct ieee80211_max_chan_switch_time_ie *mcst_ie = NULL;

    /* While IEEE80211_F_CHANSWITCH is set, insert chan switch IEs in 2 cases
     * 1) Adding the CSA IE for the first time
     * 2) We haven't sent out all the CSAs, but beacon reinit happens.
     */
    if (!vap->iv_chanchange_count || vap->beacon_reinit_done) {

        uint8_t *tempbuf;
        uint8_t csmode = IEEE80211_CSA_MODE_STA_TX_ALLOWED;
        /* the length of csa, ecsa and max chan switch time(mcst) ies
         * is represented by csa_ecsa_mcst_len, but it is initialised
         * with csa length and based on the presence of ecsa and mcst
         * the length is increased.
         */
        uint8_t csa_ecsa_mcst_len = IEEE80211_CHANSWITCHANN_BYTES;

        ieee80211vap_set_flag(vap, IEEE80211_F_CHANSWITCH);
        vap->channel_switch_state = 1;

        if (bo->bo_chanswitch[0] != IEEE80211_ELEMID_CHANSWITCHANN) {
            if (vap->iv_csmode == IEEE80211_CSA_MODE_AUTO) {

                /* No user preference for csmode. Use default behavior.
                 * If chan swith is triggered because of radar found,
                 * ask associated stations to stop transmission by
                 * sending csmode as 1 else let them transmit as usual
                 * by sending csmode as 0.
                 */
                if (ic->ic_flags & IEEE80211_F_DFS_CHANSWITCH_PENDING) {
                    /* Request STA's to stop transmission */
                    csmode = IEEE80211_CSA_MODE_STA_TX_RESTRICTED;
                }
            } else {
                /* User preference for csmode is configured.
                 * Use user preference.
                 */
                csmode = vap->iv_csmode;
            }

            /* Check if ecsa IE has to be added.
             * If yes, adjust csa_ecsa_mcst_len to include CSA IE len
             * and ECSA IE len.
             */
            if (vap->iv_enable_ecsaie) {
                csa_ecsa_mcst_len += IEEE80211_EXTCHANSWITCHANN_BYTES;
                ecsa_ie = (struct ieee80211_extendedchannelswitch_ie *)
                    ((u_int8_t*) bo->bo_chanswitch +
                     IEEE80211_CHANSWITCHANN_BYTES);
            }

            /* Check if max chan switch time IE(mcst IE) has to be added.
             * If yes, adjust csa_ecsa_mcst_len to include CSA IE len,
             * ECSA IE len and mcst IE len.
             */
            if (vap->iv_enable_max_ch_sw_time_ie) {
                mcst_ie = (struct ieee80211_max_chan_switch_time_ie *)
                    ((u_int8_t*) bo->bo_chanswitch+ csa_ecsa_mcst_len);
                csa_ecsa_mcst_len += IEEE80211_MAXCHANSWITCHTIME_BYTES;
            }

            /* Copy out trailer to open up a slot */
            tempbuf = OS_MALLOC(vap->iv_ic->ic_osdev,
                    bo->bo_chanswitch_trailerlen,
                    GFP_KERNEL);
            if (!tempbuf) {
                qdf_print("%s : tempbuf is NULL", __func__);
                return;
            }

            OS_MEMCPY(tempbuf, bo->bo_chanswitch,
                    bo->bo_chanswitch_trailerlen);

            OS_MEMCPY(bo->bo_chanswitch + csa_ecsa_mcst_len,
                    tempbuf, bo->bo_chanswitch_trailerlen);
            OS_FREE(tempbuf);

            /* Add ie in opened slot */
            bo->bo_chanswitch[0] = IEEE80211_ELEMID_CHANSWITCHANN;
            bo->bo_chanswitch[1] = 3; /* fixed length */
            bo->bo_chanswitch[2] = csmode;
            bo->bo_chanswitch[3] = ic->ic_chanchange_chan;
            bo->bo_chanswitch[4] = ic->ic_chanchange_tbtt - vap->iv_chanchange_count;

            ic->ic_num_csa_bcn_tmp_sent++;

            /* Check for ecsa_ie pointer instead of ic->ic_ecsaie flag
             * to avoid ic->ic_ecsaie being updated in between from IOCTL
             * context.
             */
            if (ecsa_ie) {
                ecsa_ie->ie = IEEE80211_ELEMID_EXTCHANSWITCHANN;
                ecsa_ie->len = 4;
                ecsa_ie->switchmode = csmode;

                /* If user configured opClass is set, use it else
                 * calculate new opClass from destination channel.
                 */
                if (vap->iv_ecsa_opclass)
                    ecsa_ie->newClass = vap->iv_ecsa_opclass;
                else
                    ecsa_ie->newClass = regdmn_get_new_opclass_from_channel(ic,
                            ic->ic_chanchange_channel);

                ecsa_ie->newchannel = ic->ic_chanchange_chan;
                ecsa_ie->tbttcount = ic->ic_chanchange_tbtt;
            }

            if (mcst_ie) {
                ieee80211_add_max_chan_switch_time(vap, (uint8_t *)mcst_ie);
            }

            /* Update the trailer lens */
            bo->bo_chanswitch_trailerlen += csa_ecsa_mcst_len;
            if (bo->bo_quiet)
                bo->bo_quiet += csa_ecsa_mcst_len;

#if ATH_SUPPORT_IBSS_DFS
            if (bo->bo_ibssdfs)
                bo->bo_ibssdfs += csa_ecsa_mcst_len;
#endif

            bo->bo_tim_trailerlen += csa_ecsa_mcst_len;

            if (bo->bo_pwrcnstr)
                bo->bo_pwrcnstr += csa_ecsa_mcst_len;

            if (bo->bo_wme)
                bo->bo_wme += csa_ecsa_mcst_len;

            if (bo->bo_erp)
                bo->bo_erp += csa_ecsa_mcst_len;

            if (bo->bo_ath_caps)
                bo->bo_ath_caps += csa_ecsa_mcst_len;

            if (bo->bo_apriori_next_channel)
                bo->bo_apriori_next_channel += csa_ecsa_mcst_len;

            if (bo->bo_appie_buf)
                bo->bo_appie_buf += csa_ecsa_mcst_len;

            if(bo->bo_whc_apinfo)
                bo->bo_whc_apinfo += csa_ecsa_mcst_len;

#if QCN_IE
            if(bo->bo_qcn_ie)
                bo->bo_qcn_ie += csa_ecsa_mcst_len;
#endif
#if QCN_ESP_IE
            if(bo->bo_esp_ie)
                bo->bo_esp_ie += csa_ecsa_mcst_len;
#endif

            if (bo->bo_ap_chan_rpt)
                bo->bo_ap_chan_rpt += csa_ecsa_mcst_len;

            if (bo->bo_rnr)
                bo->bo_rnr += csa_ecsa_mcst_len;

            if (bo->bo_mbo_cap)
                bo->bo_mbo_cap += csa_ecsa_mcst_len;

            if (bo->bo_xr)
                bo->bo_xr += csa_ecsa_mcst_len;

            if (bo->bo_secchanoffset)
                bo->bo_secchanoffset += csa_ecsa_mcst_len;

            if((bo->bo_htinfo) && (bo->bo_htcap)) {
                bo->bo_htinfo += csa_ecsa_mcst_len;
                bo->bo_htcap += csa_ecsa_mcst_len;
            }

            if (bo->bo_htinfo_pre_ana)
                bo->bo_htinfo_pre_ana += csa_ecsa_mcst_len;

            if (bo->bo_htinfo_vendor_specific)
                bo->bo_htinfo_vendor_specific += csa_ecsa_mcst_len;

            if (bo->bo_extcap)
                bo->bo_extcap += csa_ecsa_mcst_len;

            if(bo->bo_obss_scan)
                bo->bo_obss_scan += csa_ecsa_mcst_len;

            if (bo->bo_qbssload)
                bo->bo_qbssload += csa_ecsa_mcst_len;

#if UMAC_SUPPORT_WNM
            if (bo->bo_fms_desc)
                bo->bo_fms_desc += csa_ecsa_mcst_len;

            if (bo->bo_fms_trailer)
                bo->bo_fms_trailer += csa_ecsa_mcst_len;
#endif
            if ((bo->bo_vhtcap) && (bo->bo_vhtop)) {
                bo->bo_vhtcap += csa_ecsa_mcst_len;
                bo->bo_vhtop += csa_ecsa_mcst_len;
            }
            if (bo->bo_ext_bssload)
                bo->bo_ext_bssload += csa_ecsa_mcst_len;

            if((bo->bo_hecap) && (bo->bo_heop)) {
                bo->bo_hecap += csa_ecsa_mcst_len;
                bo->bo_heop += csa_ecsa_mcst_len;
            }

            if(bo->bo_vhttxpwr)
                bo->bo_vhttxpwr += csa_ecsa_mcst_len;

            if(bo->bo_vhtchnsw)
                bo->bo_vhtchnsw += csa_ecsa_mcst_len;

            if(bo->bo_interop_vhtcap)
                bo->bo_interop_vhtcap += csa_ecsa_mcst_len;

            /* Indicate new beacon length so other layers may manage memory */
            wbuf_append(wbuf, csa_ecsa_mcst_len);

            /* Indicate new beacon length so other layers may manage memory */
            *len_changed = 1;
        }

        /* Add secondary channel offset element if new channel has
         * secondary 20 MHz channel
         */
        if (((IEEE80211_IS_CHAN_11N(vap->iv_bsschan) ||
            IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
            IEEE80211_IS_CHAN_11AX(vap->iv_bsschan)) &&
            (ic->ic_chanchange_secoffset)) &&
            ic->ic_sec_offsetie && bo->bo_secchanoffset) {

            /* Add secondary channel offset element */
            struct ieee80211_ie_sec_chan_offset *sec_chan_offset_ie = NULL;
            tempbuf = OS_MALLOC(vap->iv_ic->ic_osdev,
                    bo->bo_secchanoffset_trailerlen,
                    GFP_KERNEL);

            if(tempbuf) {
                OS_MEMCPY(tempbuf, bo->bo_secchanoffset,
                        bo->bo_secchanoffset_trailerlen);

                OS_MEMCPY(bo->bo_secchanoffset +
                        IEEE80211_SEC_CHAN_OFFSET_BYTES,
                        tempbuf, bo->bo_secchanoffset_trailerlen);

                OS_FREE(tempbuf);

                sec_chan_offset_ie = (struct ieee80211_ie_sec_chan_offset *)
                    bo->bo_secchanoffset;
                sec_chan_offset_ie->elem_id = IEEE80211_ELEMID_SECCHANOFFSET;

                /* Element has only one octet of info */
                sec_chan_offset_ie->len = 1;
                sec_chan_offset_ie->sec_chan_offset =
                    ic->ic_chanchange_secoffset;

                /* Update the pointers following this element and also trailer
                 * length.
                 */
                bo->bo_secchanoffset_trailerlen +=
                    IEEE80211_SEC_CHAN_OFFSET_BYTES;

                bo->bo_chanswitch_trailerlen +=
                    IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_quiet)
                    bo->bo_quiet += IEEE80211_SEC_CHAN_OFFSET_BYTES;
                bo->bo_tim_trailerlen += IEEE80211_SEC_CHAN_OFFSET_BYTES;

#if ATH_SUPPORT_IBSS_DFS
                bo->bo_ibssdfs_trailerlen += IEEE80211_SEC_CHAN_OFFSET_BYTES;
#endif

                if (bo->bo_htinfo_pre_ana)
                    bo->bo_htinfo_pre_ana += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_obss_scan)
                    bo->bo_obss_scan += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_qbssload)
                    bo->bo_qbssload += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_extcap)
                    bo->bo_extcap += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if ((bo->bo_vhtcap) && (bo->bo_vhtop)) {
                    bo->bo_vhtcap += IEEE80211_SEC_CHAN_OFFSET_BYTES;
                    bo->bo_vhtop += IEEE80211_SEC_CHAN_OFFSET_BYTES;
                }
                if (bo->bo_ext_bssload)
                    bo->bo_ext_bssload += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if((bo->bo_hecap) && (bo->bo_heop)) {
                    bo->bo_hecap += IEEE80211_SEC_CHAN_OFFSET_BYTES;
                    bo->bo_heop += IEEE80211_SEC_CHAN_OFFSET_BYTES;
                }

                if(bo->bo_vhttxpwr)
                    bo->bo_vhttxpwr += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if(bo->bo_vhtchnsw)
                    bo->bo_vhtchnsw += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if(bo->bo_interop_vhtcap)
                    bo->bo_interop_vhtcap += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_wme)
                    bo->bo_wme += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_htinfo_vendor_specific)
                    bo->bo_htinfo_vendor_specific +=
                        IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_bwnss_map != NULL)
                    bo->bo_bwnss_map += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_ath_caps)
                    bo->bo_ath_caps += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_apriori_next_channel)
                    bo->bo_apriori_next_channel +=
                        IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_xr)
                    bo->bo_xr += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_appie_buf)
                    bo->bo_appie_buf += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if(bo->bo_whc_apinfo)
                    bo->bo_whc_apinfo += IEEE80211_SEC_CHAN_OFFSET_BYTES;

#if QCN_IE
                if(bo->bo_qcn_ie)
                    bo->bo_qcn_ie += IEEE80211_SEC_CHAN_OFFSET_BYTES;
#endif
#if QCN_ESP_IE
                if(bo->bo_esp_ie)
                    bo->bo_esp_ie += IEEE80211_SEC_CHAN_OFFSET_BYTES;
#endif

                if (bo->bo_ap_chan_rpt)
                    bo->bo_ap_chan_rpt += IEEE80211_SEC_CHAN_OFFSET_BYTES;
                if (bo->bo_rnr)
                    bo->bo_rnr += IEEE80211_SEC_CHAN_OFFSET_BYTES;
                if (bo->bo_mbo_cap)
                    bo->bo_mbo_cap += IEEE80211_SEC_CHAN_OFFSET_BYTES;

#if ATH_SUPPORT_IBSS_DFS
                bo->bo_ibssdfs_trailerlen += IEEE80211_SEC_CHAN_OFFSET_BYTES;
#endif

#if UMAC_SUPPORT_WNM
                if (bo->bo_fms_desc)
                    bo->bo_fms_desc += IEEE80211_SEC_CHAN_OFFSET_BYTES;

                if (bo->bo_fms_trailer)
                    bo->bo_fms_trailer += IEEE80211_SEC_CHAN_OFFSET_BYTES;
#endif

                /* Indicate new beacon length so other layers may
                 * manage memory.
                 */
                wbuf_append(wbuf, IEEE80211_SEC_CHAN_OFFSET_BYTES);

                /* Indicate new beacon length so other layers may
                 * manage memory.
                 */
                *len_changed = 1;
            }
        }

        /* Filling channel switch wrapper element */
        if ((IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
                IEEE80211_IS_CHAN_11AXA(vap->iv_bsschan)) &&
                ieee80211vap_vhtallowed(vap)
                && (ic->ic_chanchange_channel != NULL) &&
                (bo->bo_vhtchnsw != NULL)) {

            uint8_t *vhtchnsw_ie, vhtchnsw_ielen;

            /* Copy out trailer to open up a slot */
            tempbuf = OS_MALLOC(vap->iv_ic->ic_osdev,
                    bo->bo_vhtchnsw_trailerlen,
                    GFP_KERNEL);

            if(tempbuf != NULL) {
                OS_MEMCPY(tempbuf, bo->bo_vhtchnsw, bo->bo_vhtchnsw_trailerlen);

                /* Adding channel switch wrapper element */
                vhtchnsw_ie = ieee80211_add_chan_switch_wrp(bo->bo_vhtchnsw,
                        ni, ic, IEEE80211_FC0_SUBTYPE_BEACON,
                        /* When switching to new country by sending ECSA IE,
                         * new country IE should be also be added.
                         * As of now we dont support switching to new country
                         * without bringing down vaps so new country IE is not
                         * required.
                         */
                        (/*ecsa_ie ? IEEE80211_VHT_EXTCH_SWITCH :*/
                         !IEEE80211_VHT_EXTCH_SWITCH));
                vhtchnsw_ielen = vhtchnsw_ie - bo->bo_vhtchnsw;

                /* Copying the rest of beacon buffer */
                OS_MEMCPY(vhtchnsw_ie, tempbuf, bo->bo_vhtchnsw_trailerlen);
                OS_FREE(tempbuf);

                if(vhtchnsw_ielen) {
                    if (bo->bo_wme)
                        bo->bo_wme += vhtchnsw_ielen;

                    if (bo->bo_ath_caps)
                        bo->bo_ath_caps += vhtchnsw_ielen;

                    if(bo->bo_apriori_next_channel)
                        bo->bo_apriori_next_channel += vhtchnsw_ielen;

                    if (bo->bo_appie_buf)
                        bo->bo_appie_buf += vhtchnsw_ielen;

                    if(bo->bo_whc_apinfo)
                        bo->bo_whc_apinfo += vhtchnsw_ielen;

#if QCN_IE
                    if(bo->bo_qcn_ie)
                        bo->bo_qcn_ie += vhtchnsw_ielen;
#endif
#if QCN_ESP_IE
                    if(bo->bo_esp_ie)
                        bo->bo_esp_ie += vhtchnsw_ielen;
#endif

                    if (bo->bo_ap_chan_rpt)
                        bo->bo_ap_chan_rpt += vhtchnsw_ielen;
                    if (bo->bo_rnr)
                        bo->bo_rnr += vhtchnsw_ielen;
                    if (bo->bo_mbo_cap)
                        bo->bo_mbo_cap += vhtchnsw_ielen;

                    if (bo->bo_xr)
                        bo->bo_xr += vhtchnsw_ielen;

                    if (bo->bo_htinfo_vendor_specific)
                        bo->bo_htinfo_vendor_specific += vhtchnsw_ielen;

                    bo->bo_tim_trailerlen += vhtchnsw_ielen;
                    bo->bo_chanswitch_trailerlen += vhtchnsw_ielen;
                    bo->bo_vhtchnsw_trailerlen += vhtchnsw_ielen;
                    bo->bo_secchanoffset_trailerlen += vhtchnsw_ielen;

#if UMAC_SUPPORT_WNM
                    if (bo->bo_fms_trailer)
                        bo->bo_fms_trailerlen += vhtchnsw_ielen;
#endif

                    /* Indicate new beacon length so other layers may manage
                     * memory.
                     */
                    wbuf_append(wbuf, vhtchnsw_ielen);
                    *len_changed = 1;
                }
            }
        }
    } else {
        bo->bo_chanswitch[4] =
            ic->ic_chanchange_tbtt - vap->iv_chanchange_count;
        /* ECSA IE is added just afer CSA IE.
         * Update tbtt count in ECSA IE to same as CSA IE.
         */
        ecsa_ie = (struct ieee80211_extendedchannelswitch_ie *)
            ((uint8_t *)bo->bo_chanswitch + IEEE80211_CHANSWITCHANN_BYTES);

        if (ecsa_ie->ie == IEEE80211_ELEMID_EXTCHANSWITCHANN) {
            /* ECSA is inserted, so update tbttcount */
            ecsa_ie->tbttcount = bo->bo_chanswitch[4];
        }
    }

    vap->iv_chanchange_count++;

    /* In case of repeater move, send deauth to old root AP one count
     * before channel switch happens
     */
    if (bo->bo_chanswitch[4] == 1 && ic->ic_repeater_move.state == REPEATER_MOVE_START) {
        struct ieee80211vap *rep_sta_vap = ic->ic_sta_vap;
        struct ieee80211_node *ni = ieee80211_vap_find_node(rep_sta_vap, rep_sta_vap->iv_bss->ni_bssid);
        if (ni != NULL) {
            ieee80211_send_deauth(ni, IEEE80211_REASON_AUTH_LEAVE);
            ieee80211_free_node(ni);
        }
    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
            "%s: CHANSWITCH IE, change in %d \n",
            __func__, bo->bo_chanswitch[4]);
}

static void ieee80211_beacon_reinit(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed,
        bool *update_beacon_copy)
{
    uint8_t *frm = NULL;
    struct ieee80211vap *vap = ni->ni_vap;

    frm = (uint8_t *) wbuf_header(wbuf) + sizeof(struct ieee80211_frame);
    frm = ieee80211_beacon_init(ni, bo, frm);
    if (!frm)
        return;

    *update_beacon_copy = true;
    wbuf_set_pktlen(wbuf, (frm - (uint8_t *)wbuf_header(wbuf)));
    *len_changed = 1;
    vap->beacon_reinit_done = true;
}

static void ieee80211_csa_interop_phy_iter_sta(void *arg, wlan_node_t wn)
{
    struct ieee80211_node *bss;
    struct ieee80211_node *ni;
    struct ieee80211vap *vap;
    struct ieee80211com *ic;

    ni = wn;
    bss = arg;

    if (!ni || !bss) {
        return;
    }
    vap = ni->ni_vap;
    ic = vap->iv_ic;

    if (ni == bss) {
        qdf_debug("[%s, %pM] skipping bss node", vap->iv_netdev_name,
                 ni->ni_macaddr);
        return;
    }

    ieee80211_csa_interop_phy_update(ni, -1);
}

static void ieee80211_csa_interop_phy_iter_vap(void *arg, wlan_if_t wif)
{
    struct ieee80211vap *vap;

    vap = wif;
    if (!vap->iv_csa_interop_phy)
        return;

    wlan_iterate_station_list(vap, ieee80211_csa_interop_phy_iter_sta, vap->iv_bss);
}

static void ieee80211_csa_interop_phy(struct ieee80211com *ic)
{
    int err;

    /* Subscribe to ppdu stats to see if STA is transmitting
     * higher bw frames.
     */
    if (ic->ic_subscribe_csa_interop_phy &&
            ieee80211_get_num_csa_interop_phy_vaps(ic)) {
        err = ic->ic_subscribe_csa_interop_phy(ic, true);
        if (!err) {
            wlan_iterate_vap_list(ic, ieee80211_csa_interop_phy_iter_vap, NULL);
            /* start timer to unsubscrive per ppdu stats */
            qdf_timer_mod(&ic->ic_csa_max_rx_wait_timer, g_csa_max_rx_wait_time);
        }
    }
}

/* ieee80211_change_channel: If all the CSAs have been sent, change the channel
 * and reset the channel switch flags.
 *
 * return 0: Perform channel change and reset channel switch flags.
 * return 1: Not all the CSAs have been sent or usenol is 0, so channel change
 * doesn't happen.
 */
static int ieee80211_change_channel(
        struct ieee80211_node *ni,
        bool *update_beacon_copy,
        int *len_changed,
        wbuf_t wbuf,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_ath_channel *c;
    struct ieee80211vap *tmp_vap = NULL;
    struct ieee80211_vap_opmode_count vap_opmode_count;

    if ((vap->iv_flags & IEEE80211_F_CHANSWITCH) &&
            (vap->iv_chanchange_count == ic->ic_chanchange_tbtt) &&
            IEEE80211_CHANCHANGE_BY_BEACONUPDATE_IS_SET(ic)) {

        vap->iv_chanchange_count = 0;

        /*
         * NB: iv_bsschan is in the DSPARMS beacon IE, so must set this
         * prior to the beacon re-init, below.
         */
        if (!ic->ic_chanchange_channel) {
            c = ieee80211_doth_findchan(vap, ic->ic_chanchange_chan);
            if (c == NULL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
                        "%s: find channel failure\n", __func__);
                return 0;
            }
        } else {
            c = ic->ic_chanchange_channel;
        }
        vap->iv_bsschan = c;

#if ATH_SUPPORT_IBSS_DFS
        if (vap->iv_opmode == IEEE80211_M_IBSS) {
            if (IEEE80211_ADDR_EQ(vap->iv_ibssdfs_ie_data.owner,
                        vap->iv_myaddr))
                vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_OWNER;
            else
                vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_JOINER;
        }
#endif

        ieee80211_beacon_reinit(ni, bo, wbuf, len_changed, update_beacon_copy);

        /* Clear IEEE80211_F_CHANSWITCH flag */
        ieee80211vap_clear_flag(vap, IEEE80211_F_CHANSWITCH);

        ieee80211com_clear_flags(ic, IEEE80211_F_CHANSWITCH);

        ieee80211_csa_interop_phy(ic);

        if (ic->ic_chanchange_chwidth != 0) {
            /* Wide Bandwidth Channel Switch for VHT/11ax 5 GHz only.
             * In this case need to update phymode.
             */
            uint64_t chan_flag = ic->ic_chanchange_chanflag;
            enum ieee80211_phymode mode = 0;

            /* 11AX TODO: Recheck future 802.11ax drafts (>2.0) on
             * channel switching rules.
             */

            /*Get phymode from chan_flag value */
            mode = ieee80211_get_phymode_from_chan_flag(ic->ic_curchan,
                    chan_flag);

            if(mode != 0 && (ic->ic_opmode == IEEE80211_M_HOSTAP)){
                ieee80211_setmode(ic, mode, IEEE80211_M_HOSTAP);
                OS_MEMZERO(&vap_opmode_count, sizeof(vap_opmode_count));
                ieee80211_get_vap_opmode_count(ic, &vap_opmode_count);
                /* Allow phymode override in non-repeater mode */
                if (!vap_opmode_count.sta_count) {
                    TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
                        wlan_set_desired_phymode(tmp_vap,mode);
                    }
                }

            }
        }

        if (ic->ic_curchan != c) {
            ieee80211_chan_switch_to_new_chan(vap, c);
        } else {
            TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
                if(tmp_vap->iv_opmode == IEEE80211_M_HOSTAP) {
                    tmp_vap->iv_remove_csa_ie = true;
                    tmp_vap->iv_no_restart = true;
                    tmp_vap->iv_chanchange_count = 0;
                    ieee80211vap_clear_flag(tmp_vap, IEEE80211_F_CHANSWITCH);
                    wlan_vdev_mlme_sm_deliver_evt(tmp_vap->vdev_obj,
                                        WLAN_VDEV_SM_EV_CSA_COMPLETE, 0, NULL);
                }
            }
        }

        /* Resetting VHT channel change variables */
        ic->ic_chanchange_channel = NULL;
        ic->ic_chanchange_chwidth = 0;
        IEEE80211_CHAN_SWITCH_END(ic);

        IEEE80211_CHANCHANGE_STARTED_CLEAR(ic);
        IEEE80211_CHANCHANGE_BY_BEACONUPDATE_CLEAR(ic);
        if(mlme_dfs_get_rn_use_nol(ic->ic_pdev_obj))
            return 0;
    }

    return 1;
}

static void inline ieee80211_update_capinfo(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211com *ic = vap->iv_ic;
    uint16_t capinfo;

    /* XXX faster to recalculate entirely or just changes? */
    if (vap->iv_opmode == IEEE80211_M_IBSS){
        if(ic->ic_softap_enable)
            capinfo = IEEE80211_CAPINFO_ESS;
        else
            capinfo = IEEE80211_CAPINFO_IBSS;
    } else {
        capinfo = IEEE80211_CAPINFO_ESS;
    }

    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
        capinfo |= IEEE80211_CAPINFO_PRIVACY;

    if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
            IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan))
        capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;

    if (ic->ic_flags & IEEE80211_F_SHSLOT)
        capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;

    if (ieee80211_ic_doth_is_set(ic) &&
            ieee80211_vap_doth_is_set(vap))
        capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;

    if (IEEE80211_VAP_IS_PUREB_ENABLED(vap))
        capinfo &= ~IEEE80211_CAPINFO_SHORT_SLOTTIME;

    /* set rrm capbabilities, if supported */
    if (ieee80211_vap_rrm_is_set(vap))
        capinfo |= IEEE80211_CAPINFO_RADIOMEAS;

    *bo->bo_caps = htole16(capinfo);
}

/*
 * Check if channel change due to CW interference needs to be done.
 * Since this is a drastic channel change, we do not wait for the TBTT
 * interval to expair and do not send Channel change flag in beacon.
 */
static void ieee80211_beacon_check_and_reinit_beacon(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed,
        bool *update_beacon_copy)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;

    if ((vap->iv_flags_ext2 & IEEE80211_FEXT2_BR_UPDATE) ||
            vap->iv_update_vendor_ie ||
            vap->channel_change_done ||
            vap->appie_buf_updated   ||
            vap->iv_doth_updated     ||
            (vap->iv_flags_ext2 & IEEE80211_FEXT2_MBO) ||
            vap->iv_remove_csa_ie    ||
            vap->iv_he_bsscolor_remove_ie ||
            vap->iv_mbss.mbssid_add_del_profile
	) {

        ieee80211_beacon_reinit(ni, bo, wbuf, len_changed, update_beacon_copy);

        ieee80211vap_clear_flag_ext2(vap, IEEE80211_FEXT2_MBO);
        ieee80211vap_clear_flag_ext2(vap, IEEE80211_FEXT2_BR_UPDATE);
        vap->iv_update_vendor_ie = 0;
        vap->channel_change_done = 0;
        vap->appie_buf_updated   = 0;
        vap->iv_doth_updated     = 0;
        vap->iv_he_bsscolor_remove_ie = false;
        vap->iv_mbss.mbssid_add_del_profile = 0;

        if (!vap->iv_bcn_offload_enable) {
            vap->iv_remove_csa_ie = false;
            vap->channel_switch_state = 0;
        }

        if (ic->cw_inter_found)
            ic->cw_inter_found = 0;
    }
}

static void ieee80211_beacon_add_wme_param(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo,
        bool *update_beacon_copy)
{
    if (ieee80211_vap_wme_is_set(vap) &&
#if ATH_SUPPORT_IBSS_WMM
            (vap->iv_opmode == IEEE80211_M_HOSTAP ||
             vap->iv_opmode == IEEE80211_M_IBSS)
#else
            (vap->iv_opmode == IEEE80211_M_HOSTAP)
#endif
       ) {
        struct ieee80211_wme_state *wme = &vap->iv_wmestate;

        /* XXX multi-bss */
        if ((vap->iv_flags & IEEE80211_F_WMEUPDATE) && (bo->bo_wme)) {
            ieee80211_add_wme_param(bo->bo_wme, wme,
                    IEEE80211_VAP_IS_UAPSD_ENABLED(vap));
            *update_beacon_copy = true;
            ieee80211vap_clear_flag(vap, IEEE80211_F_WMEUPDATE);
        }
    }
}

static void ieee80211_beacon_update_muedca_param(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo,
        bool *update_beacon_copy)
{
    if(ieee80211_vap_wme_is_set(vap) &&
            ieee80211vap_heallowed(vap) &&
            ieee80211vap_muedca_is_enabled(vap) && (bo->bo_muedca)) {
        ieee80211_add_muedca_param(bo->bo_muedca, &vap->iv_muedcastate);
        *update_beacon_copy = true;
    }
}

static void  ieee80211_beacon_update_pwrcnstr(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (bo->bo_pwrcnstr &&
            ieee80211_ic_doth_is_set(ic) &&
            ieee80211_vap_doth_is_set(vap)) {
        uint8_t *pwrcnstr = bo->bo_pwrcnstr;

        *pwrcnstr++ = IEEE80211_ELEMID_PWRCNSTR;
        *pwrcnstr++ = 1;
        *pwrcnstr++ = IEEE80211_PWRCONSTRAINT_VAL(vap);
    }
}

static void ieee80211_update_chan_utilization(
        struct ieee80211vap *vap)
{
#if UMAC_SUPPORT_QBSSLOAD
    ieee80211_beacon_chanutil_update(vap);
#elif UMAC_SUPPORT_CHANUTIL_MEASUREMENT
    if (vap->iv_chanutil_enab) {
        ieee80211_beacon_chanutil_update(vap);
    }
#endif
}

static void inline ieee80211_beacon_add_htcap(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int enable_htrates;

    enable_htrates = ieee80211vap_htallowed(vap);

    /*
     * HT cap. check for vap is done in ieee80211vap_htallowed.
     * TBD: remove iv_bsschan check to support multiple channel operation.
     */
    if (ieee80211_vap_wme_is_set(vap) &&
            (IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) ||
             IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
             IEEE80211_IS_CHAN_11N(vap->iv_bsschan)) &&
            enable_htrates && (bo->bo_htinfo != NULL) &&
            (bo->bo_htcap != NULL)) {

        struct ieee80211_ie_htinfo_cmn *htinfo;
        struct ieee80211_ie_obss_scan *obss_scan;

#if IEEE80211_BEACON_NOISY
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N,
                "%s: AP: updating HT Info IE (ANA) for %s\n",
                __func__, ether_sprintf(ni->ni_macaddr));

        if (bo->bo_htinfo[0] != IEEE80211_ELEMID_HTINFO_ANA) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N,
                    "%s: AP: HT Info IE (ANA) beacon offset askew %s "
                    "expected 0x%02x, found 0x%02x\n",
                    __func__, ether_sprintf(ni->ni_macaddr),
                    IEEE80211_ELEMID_HTINFO_ANA, bo->bo_htinfo[0]);
        }
#endif
        htinfo = &((struct ieee80211_ie_htinfo *)bo->bo_htinfo)->hi_ie;
        ieee80211_update_htinfo_cmn(htinfo, ni);

        ieee80211_add_htcap(bo->bo_htcap, ni, IEEE80211_FC0_SUBTYPE_BEACON);

        if (!(ic->ic_flags & IEEE80211_F_COEXT_DISABLE)) {
            obss_scan = (struct ieee80211_ie_obss_scan *)bo->bo_obss_scan;
            if(obss_scan)
                ieee80211_update_obss_scan(obss_scan, ni);
        }

        if (IEEE80211_IS_HTVIE_ENABLED(ic) &&
                bo->bo_htinfo_vendor_specific) {
#if IEEE80211_BEACON_NOISY
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N,
                    "%s: AP: updating HT Info IE (Vendor Specific) for %s\n",
                    __func__, ether_sprintf(ni->ni_macaddr));
            if (bo->bo_htinfo_vendor_specific[5] !=
                    IEEE80211_ELEMID_HTINFO_VENDOR) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N,
                        "%s: AP: HT Info IE (Vendor Specific) beacon offset askew %s expected 0x%02x, found 0x%02x\n",
                        __func__, ether_sprintf(ni->ni_macaddr),
                        IEEE80211_ELEMID_HTINFO_ANA,
                        bo->bo_htinfo_vendor_specific[5] );
            }
#endif
            htinfo = &((struct vendor_ie_htinfo *)
                    bo->bo_htinfo_vendor_specific)->hi_ie;
            ieee80211_update_htinfo_cmn(htinfo, ni);
        }
    }
}

static void inline ieee80211_beacon_add_vhtcap(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;

    /* Add VHT cap if device is in 11ac operating mode (or)
     * 256QAM is enabled in 2.4G.
     */
    if (ieee80211_vap_wme_is_set(vap) &&
            (IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) ||
             IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
             IEEE80211_IS_CHAN_11NG(vap->iv_bsschan)) &&
            ieee80211vap_vhtallowed(vap) &&
            (bo->bo_vhtcap != NULL) && (bo->bo_vhtop != NULL)) {

        /* Add VHT capabilities IE */
        ieee80211_add_vhtcap(bo->bo_vhtcap, ni, ic,
                IEEE80211_FC0_SUBTYPE_BEACON, NULL, NULL);

        /* Add VHT Operation IE */
        ieee80211_add_vhtop(bo->bo_vhtop, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON,
                NULL);

        /* Add VHT Tx Power Envelope IE */
        if (bo->bo_vhttxpwr && ieee80211_ic_doth_is_set(ic) &&
                ieee80211_vap_doth_is_set(vap)) {
            ieee80211_add_vht_txpwr_envlp(bo->bo_vhttxpwr, ni, ic,
                    IEEE80211_FC0_SUBTYPE_BEACON,
                    !IEEE80211_VHT_TXPWR_IS_SUB_ELEMENT);
        }
    }

    /* Add VHT Vendor specific IE for 256QAM support in 2.4G Interop */
    if (ieee80211_vap_wme_is_set(vap) &&
            IEEE80211_IS_CHAN_11NG(vap->iv_bsschan) &&
            ieee80211vap_vhtallowed(vap) &&
            ieee80211vap_11ng_vht_interopallowed(vap) &&
            (bo->bo_interop_vhtcap != NULL)) {
        /* Add VHT capabilities IE and VHT OP IE */
        ieee80211_add_interop_vhtcap(bo->bo_interop_vhtcap, ni, ic,
                IEEE80211_FC0_SUBTYPE_BEACON);
    }
}

static void ieee80211_add_he_cap(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic  = ni->ni_ic;

    if (ieee80211_vap_wme_is_set(vap) &&
            IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) &&
            ieee80211vap_heallowed(vap) &&
            (bo->bo_hecap != NULL) &&
            (bo->bo_heop != NULL)) {

        /* Add HE capabilities IE */
        ieee80211_add_hecap(bo->bo_hecap, ni, ic,
                IEEE80211_FC0_SUBTYPE_BEACON);

        /* Add HE Operation IE */
        ieee80211_add_heop(bo->bo_heop, ni, ic, IEEE80211_FC0_SUBTYPE_BEACON,
                NULL);
    }
}

static void ieee80211_beacon_add_bsscolor_change_ie(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int  *len_changed)
{
    struct ieee80211vap *vap = ni->ni_vap;
    uint8_t vdev_id =
           ((struct ol_ath_vap_net80211 *)OL_ATH_VAP_NET80211(vap))->av_if_id;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
        "%s>> vdev-id: %d iv_he_bsscolor_change_ongoing: %s",  __func__,
        vdev_id, vap->iv_he_bsscolor_change_ongoing ? "true": "false");

    if (IEEE80211_IS_CHAN_11AX(vap->iv_bsschan) &&
                    ieee80211vap_heallowed(vap) &&
                    vap->iv_he_bsscolor_change_ongoing) {
        ieee80211_add_he_bsscolor_change_ie(bo, wbuf, ni,
                IEEE80211_FC0_SUBTYPE_BEACON, len_changed);
        if(vap->iv_bcca_ie_status == BCCA_NA) {
            vap->iv_bcca_ie_status = BCCA_START;
        } else {
            vap->iv_bcca_ie_status = BCCA_ONGOING;
        }
    }
    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

static void ieee80211_find_new_chan(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ieee80211_ic_doth_is_set(ic) &&
            (ic->ic_flags & IEEE80211_F_CHANSWITCH) &&
            IEEE80211_CHANCHANGE_BY_BEACONUPDATE_IS_SET(ic)) {
        if (!(ic->ic_chanchange_channel)) {
            ic->ic_chanchange_channel =
                ieee80211_doth_findchan(vap, ic->ic_chanchange_chan);
            if(!(ic->ic_chanchange_channel)) {
                /*
                 * Ideally we should not be here, Only reason is that we have
                 * a corrupt chan.
                 */
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                        "%s : Error chanchange is NULL: VAP = %d "
                        "chan = %d cfreq = %d flags = %llu",
                        __func__, vap->iv_unit, ic->ic_chanchange_chan,
                        vap->iv_des_cfreq2,
                        (vap->iv_bsschan->ic_flags & IEEE80211_CHAN_ALL));
            } else {
                /* Find secondary 20 offset to advertise in beacon */
                ic->ic_chanchange_secoffset =
                    ieee80211_sec_chan_offset(ic->ic_chanchange_channel);
                /* Find destination channel width */
                ic->ic_chanchange_chwidth =
                    ieee80211_get_chan_width(ic->ic_chanchange_channel);
            }
        }
    }
}

static void inline ieee80211_beacon_update_tim(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        struct ieee80211_ath_tim_ie **tie,
        wbuf_t wbuf,
        int *len_changed,
        int mcast,
        int *is_dtim)
{
    struct ieee80211vap *vap = ni->ni_vap;

    if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
            vap->iv_opmode == IEEE80211_M_IBSS) {
        *tie = (struct ieee80211_ath_tim_ie *) bo->bo_tim;

        if (IEEE80211_VAP_IS_TIMUPDATE_ENABLED(vap) &&
                vap->iv_opmode == IEEE80211_M_HOSTAP) {
            u_int timlen = 0;
            u_int timoff = 0;
            u_int i = 0;

            /*
             * ATIM/DTIM needs updating. If it fits in the current space
             * allocated then just copy in the new bits. Otherwise we need to
             * move any trailing data to make room. Note that we know there is
             * contiguous space because ieee80211_beacon_allocate insures there
             * is space in the wbuf to write a maximal-size virtual bitmap
             * (based on ic_max_aid).
             */
            /*
             * Calculate the bitmap size and offset, copy any trailer out of the
             * way, and then copy in the new bitmap and update the information
             * element. Note that the tim bitmap must contain at least one byte
             * and any offset must be even.
             */
            if (vap->iv_ps_pending != 0) {
                timoff = 128;        /* Impossibly large */
                for (i = 0; i < vap->iv_tim_len; i++) {
                    if (vap->iv_tim_bitmap[i]) {
                        timoff = i &~ 1;
                        break;
                    }
                }
                /* Remove the assert and do a recovery */
                /* KASSERT(timoff != 128, ("tim bitmap empty!")); */
                if (timoff == 128) {
                    timoff = 0;
                    timlen = 1;
                    qdf_print("Recover in TIM update");
                } else {
                    for (i = vap->iv_tim_len-1; i >= timoff; i--) {
                        if (vap->iv_tim_bitmap[i])
                            break;
                    }

                    if (i < timoff) {
                        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                                "Corrupted tim ie, recover in TIM update, "
                                "tim_len = %d, i = %d, timoff = %d",
                                vap->iv_tim_len, i, timoff);
                        timoff = 0;
                        timlen = 1;
                    } else {
                        timlen = 1 + (i - timoff);
                        /* Resetting the timlen if it goes beyond 68 limit
                         * (64 + 4 The 64 is to support 512 client 4 is a
                         * gaurd band.
                         */
                        if (timlen > 68) {
                            timoff = 0;
                            timlen = 1;
                            qdf_print("Recover in TIM update Invalid TIM length");
                        }
                    }
                }
            } else {
                timoff = 0;
                timlen = 1;
            }

            (*tie)->tim_bitctl = timoff;
            if (timlen != bo->bo_tim_len) {
                int trailer_adjust =
                    ((*tie)->tim_bitmap+timlen) - bo->bo_tim_trailer;

                /* copy up/down trailer */
                OS_MEMMOVE((*tie)->tim_bitmap+timlen, bo->bo_tim_trailer,
                        bo->bo_tim_trailerlen);
                bo->bo_tim_trailer = (*tie)->tim_bitmap+timlen;

                if (bo->bo_chanswitch)
                    bo->bo_chanswitch += trailer_adjust;

                if (bo->bo_pwrcnstr)
                    bo->bo_pwrcnstr += trailer_adjust;

                if (bo->bo_quiet)
                    bo->bo_quiet += trailer_adjust;

                if (bo->bo_wme)
                    bo->bo_wme += trailer_adjust;

                if (bo->bo_erp)
                    bo->bo_erp += trailer_adjust;

                if (bo->bo_ath_caps)
                    bo->bo_ath_caps += trailer_adjust;

                if (bo->bo_apriori_next_channel)
                    bo->bo_apriori_next_channel += trailer_adjust;

                if (bo->bo_appie_buf)
                    bo->bo_appie_buf += trailer_adjust;

                if(bo->bo_whc_apinfo)
                    bo->bo_whc_apinfo += trailer_adjust;

#if QCN_IE
                if(bo->bo_qcn_ie)
                    bo->bo_qcn_ie += trailer_adjust;
#endif

                if (bo->bo_ap_chan_rpt)
                    bo->bo_ap_chan_rpt += trailer_adjust;
                if (bo->bo_rnr)
                    bo->bo_rnr += trailer_adjust;
                if (bo->bo_mbo_cap)
                    bo->bo_mbo_cap += trailer_adjust;

#if QCN_ESP_IE
                if(bo->bo_esp_ie)
                    bo->bo_esp_ie += trailer_adjust;
#endif
                if (bo->bo_xr)
                    bo->bo_xr += trailer_adjust;

                if (bo->bo_secchanoffset)
                    bo->bo_secchanoffset += trailer_adjust;
#if ATH_SUPPORT_IBSS_DFS
                if (bo->bo_ibssdfs)
                    bo->bo_ibssdfs += trailer_adjust;
#endif

                if(bo->bo_htinfo && bo->bo_htcap) {
                    bo->bo_htinfo += trailer_adjust;
                    bo->bo_htcap += trailer_adjust;
                }

                if (bo->bo_htinfo_pre_ana)
                    bo->bo_htinfo_pre_ana += trailer_adjust;

                if (bo->bo_htinfo_vendor_specific)
                    bo->bo_htinfo_vendor_specific += trailer_adjust;

                if (bo->bo_extcap)
                    bo->bo_extcap += trailer_adjust;

                if(bo->bo_obss_scan)
                    bo->bo_obss_scan += trailer_adjust;

                if (bo->bo_qbssload)
                    bo->bo_qbssload += trailer_adjust;

#if UMAC_SUPPORT_WNM

                if (bo->bo_fms_desc)
                    bo->bo_fms_desc += trailer_adjust;

                if (bo->bo_fms_trailer)
                    bo->bo_fms_trailer += trailer_adjust;
#endif

                if (bo->bo_vhtcap && bo->bo_vhtop) {
                    bo->bo_vhtcap += trailer_adjust;
                    bo->bo_vhtop += trailer_adjust;
                }
                if (bo->bo_ext_bssload)
                    bo->bo_ext_bssload += trailer_adjust;

                if(bo->bo_hecap && bo->bo_heop) {
                    bo->bo_hecap += trailer_adjust;
                    bo->bo_heop += trailer_adjust;
                }

                if(bo->bo_vhttxpwr)
                    bo->bo_vhttxpwr += trailer_adjust;

                if(bo->bo_vhtchnsw)
                    bo->bo_vhtchnsw += trailer_adjust;

                if(bo->bo_interop_vhtcap)
                    bo->bo_interop_vhtcap += trailer_adjust;

                if (timlen > bo->bo_tim_len)
                    wbuf_append(wbuf, timlen - bo->bo_tim_len);
                else
                    wbuf_trim(wbuf, bo->bo_tim_len - timlen);


                bo->bo_tim_len = timlen;
                /* Update information element */
                (*tie)->tim_len = 3 + timlen;
                *len_changed = 1;
            }

            OS_MEMCPY((*tie)->tim_bitmap, vap->iv_tim_bitmap + timoff,
                    bo->bo_tim_len);

            IEEE80211_VAP_TIMUPDATE_DISABLE(vap);

            IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
                    "%s: TIM updated, pending %u, off %u, len %u\n",
                    __func__, vap->iv_ps_pending, timoff, timlen);
        }

        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            /* Count down DTIM period */
            if ((*tie)->tim_count == 0)
                (*tie)->tim_count = (*tie)->tim_period - 1;
            else
                (*tie)->tim_count--;

            /* Update state for buffered multicast frames on DTIM */
            if (mcast && ((*tie)->tim_count == 0 || (*tie)->tim_period == 1))
                (*tie)->tim_bitctl |= 1;
            else
                (*tie)->tim_bitctl &= ~1;

#if ATH_SUPPORT_WIFIPOS
            /* Enable MCAST bit to wake the station */
            if (vap->iv_wakeup & 0x2)
                (*tie)->tim_bitctl |= 1;
#endif
        }
#if UMAC_SUPPORT_WNM
        *is_dtim = ((*tie)->tim_count == 0 || (*tie)->tim_period == 1);
#endif
    }
}

static void ieee80211_send_chan_switch_action(struct ieee80211_node *ni)
{
        struct ieee80211vap *vap = ni->ni_vap;
        struct ieee80211_action_mgt_args *actionargs;

        actionargs = OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(struct ieee80211_action_mgt_args) , GFP_KERNEL);
        if (actionargs == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Unable to alloc arg buf. Size=%d\n",
                     __func__, sizeof(struct ieee80211_action_mgt_args));
        } else {
            OS_MEMZERO(actionargs, sizeof(struct ieee80211_action_mgt_args));

            actionargs->category = IEEE80211_ACTION_CAT_SPECTRUM;
            actionargs->action   = IEEE80211_ACTION_CHAN_SWITCH;
            ieee80211_send_action(ni, actionargs, NULL);
            OS_FREE(actionargs);
        }
}

static void ieee80211_beacon_add_chan_switch_ie(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;

    if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
            vap->iv_opmode == IEEE80211_M_IBSS) {

        /* Find the new channel if it's not known already */
        ieee80211_find_new_chan(vap);

        if (ieee80211_ic_doth_is_set(ic) &&
                (ic->ic_flags & IEEE80211_F_CHANSWITCH) &&
                (ic->ic_chanchange_channel) &&
                IEEE80211_CHANCHANGE_BY_BEACONUPDATE_IS_SET(ic)) {
            ieee80211_add_channel_switch_ie(ni, bo, wbuf, len_changed);
            if (*len_changed == 1)
                ieee80211_send_chan_switch_action(ni);
        }
    }
}

#if ATH_SUPPORT_IBSS_DFS
static int ieee80211_beacon_update_ibss_dfs(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_ibssdfs_ie *ibss_ie = NULL;

    if (vap->iv_opmode == IEEE80211_M_IBSS &&
            (ic->ic_curchan->ic_flagext & IEEE80211_CHAN_DFS)) {

        /*reset action frames counts for measrep and csa action */
        vap->iv_measrep_action_count_per_tbtt = 0;
        vap->iv_csa_action_count_per_tbtt = 0;

        if(vap->iv_ibssdfs_state == IEEE80211_IBSSDFS_OWNER)
            vap->iv_ibssdfs_ie_data.rec_interval =
                vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt;

        ibss_ie =(struct ieee80211_ibssdfs_ie *) bo->bo_ibssdfs;
        if(ibss_ie) {
            if (OS_MEMCMP(bo->bo_ibssdfs, &vap->iv_ibssdfs_ie_data,
                        ibss_ie->len + sizeof(struct ieee80211_ie_header))) {
                int trailer_adjust =
                    vap->iv_ibssdfs_ie_data.len - ibss_ie->len;

                /* Copy up/down trailer */
                if(trailer_adjust > 0) {
                    uint8_t	*tempbuf;

                    tempbuf = OS_MALLOC(vap->iv_ic->ic_osdev,
                            bo->bo_ibssdfs_trailerlen , GFP_KERNEL);
                    if (tempbuf == NULL) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                                "%s: Unable to alloc ibssdfs copy buf. Size=%d\n",
                                __func__, bo->bo_ibssdfs_trailerlen);
                        return -1;
                    }

                    OS_MEMCPY(tempbuf, (bo->bo_ibssdfs +
                                sizeof(struct ieee80211_ie_header) +
                                ibss_ie->len),
                            bo->bo_ibssdfs_trailerlen);

                    OS_MEMCPY((bo->bo_ibssdfs +
                                sizeof(struct ieee80211_ie_header) +
                                vap->iv_ibssdfs_ie_data.len), tempbuf,
                            bo->bo_ibssdfs_trailerlen);
                    OS_FREE(tempbuf);
                } else {
                    OS_MEMCPY((bo->bo_ibssdfs +
                                sizeof(struct ieee80211_ie_header) +
                                vap->iv_ibssdfs_ie_data.len),
                            (bo->bo_ibssdfs +
                             sizeof(struct ieee80211_ie_header) + ibss_ie->len),
                            bo->bo_ibssdfs_trailerlen);
                }

                bo->bo_tim_trailerlen += trailer_adjust;
                bo->bo_chanswitch_trailerlen += trailer_adjust;

                if (bo->bo_quiet)
                    bo->bo_quiet += trailer_adjust;

                if (bo->bo_wme)
                    bo->bo_wme += trailer_adjust;

                if (bo->bo_erp)
                    bo->bo_erp += trailer_adjust;

                if (bo->bo_ath_caps)
                    bo->bo_ath_caps += trailer_adjust;

                if (bo->bo_apriori_next_channel)
                    bo->bo_apriori_next_channel += trailer_adjust;

                if (bo->bo_appie_buf)
                    bo->bo_appie_buf += trailer_adjust;

                if(bo->bo_whc_apinfo)
                    bo->bo_whc_apinfo += trailer_adjust;

#if QCN_IE
                if(bo->bo_qcn_ie)
                    bo->bo_qcn_ie += trailer_adjust;
#endif
#if QCN_ESP_IE
                if(bo->bo_esp_ie)
                    bo->bo_esp_ie += trailer_adjust;
#endif

                if (bo->bo_ap_chan_rpt)
                    bo->bo_ap_chan_rpt += trailer_adjust;
                if (bo->bo_rnr)
                    bo->bo_rnr += trailer_adjust;
                if (bo->bo_mbo_cap)
                    bo->bo_mbo_cap += trailer_adjust;

                if (bo->bo_xr)
                    bo->bo_xr += trailer_adjust;

                if (bo->bo_secchanoffset)
                    bo->bo_secchanoffset += trailer_adjust;

                if(bo->bo_htinfo && bo->bo_htcap) {
                    bo->bo_htinfo += trailer_adjust;
                    bo->bo_htcap += trailer_adjust;
                }

                if (bo->bo_htinfo_pre_ana)
                    bo->bo_htinfo_pre_ana += trailer_adjust;

                if (bo->bo_htinfo_vendor_specific)
                    bo->bo_htinfo_vendor_specific += trailer_adjust;

                if (bo->bo_extcap)
                    bo->bo_extcap += trailer_adjust;

                if(bo->bo_obss_scan)
                    bo->bo_obss_scan += trailer_adjust;

                if (bo->bo_qbssload)
                    bo->bo_qbssload += trailer_adjust;
#if UMAC_SUPPORT_WNM
                if (bo->bo_fms_desc)
                    bo->bo_fms_desc += trailer_adjust;
                if (bo->bo_fms_trailer)
                    bo->bo_fms_trailer += trailer_adjust;
#endif

                if (bo->bo_vhtcap && bo->bo_vhtop) {
                    bo->bo_vhtcap += trailer_adjust;
                    bo->bo_vhtop += trailer_adjust;
                }
                if (bo->bo_ext_bssload)
                    bo->bo_ext_bssload += trailer_adjust;

                if(bo->bo_hecap && bo->bo_heop) {
                    bo->bo_hecap += trailer_adjust;
                    bo->bo_heop += trailer_adjust;
                }

                if(bo->bo_vhttxpwr)
                    bo->bo_vhttxpwr += trailer_adjust;

                if(bo->bo_vhtchnsw)
                    bo->bo_vhtchnsw += trailer_adjust;

                if(bo->bo_interop_vhtcap)
                    bo->bo_interop_vhtcap += trailer_adjust;

                if (trailer_adjust >  0)
                    wbuf_append(wbuf, trailer_adjust);
                else
                    wbuf_trim(wbuf, -trailer_adjust);

                OS_MEMCPY(bo->bo_ibssdfs, &vap->iv_ibssdfs_ie_data,
                        (sizeof(struct ieee80211_ie_header) +
                         vap->iv_ibssdfs_ie_data.len));
                *len_changed = 1;
            }
        }
        if (vap->iv_ibssdfs_state == IEEE80211_IBSSDFS_JOINER ||
                vap->iv_ibssdfs_state == IEEE80211_IBSSDFS_OWNER) {
            vap->iv_ibssdfs_recovery_count =
                vap->iv_ibssdfs_ie_data.rec_interval;
            ieee80211_ibss_beacon_update_stop(ic);
        } else if (vap->iv_ibssdfs_state ==
                IEEE80211_IBSSDFS_WAIT_RECOVERY) {
            vap->iv_ibssdfs_recovery_count --;

            if(vap->iv_ibssdfs_recovery_count == 0) {
                IEEE80211_ADDR_COPY(vap->iv_ibssdfs_ie_data.owner,
                        vap->iv_myaddr);
                vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_OWNER;
                vap->iv_ibssdfs_recovery_count =
                    vap->iv_ibssdfs_ie_data.rec_interval;

                if(!vap->iv_ibss_dfs_no_channel_switch) {
                    IEEE80211_RADAR_FOUND_LOCK(ic);
                    ieee80211_dfs_action(vap, NULL, false);
                    IEEE80211_RADAR_FOUND_UNLOCK(ic);
                }

                vap->iv_ibss_dfs_no_channel_switch = false;
            }
        }
    }
    return 0;
}
#endif

static void ieee80211_beacon_erp_update(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (((vap->iv_opmode == IEEE80211_M_HOSTAP) &&
                (IEEE80211_IS_CHAN_ANYG(vap->iv_bsschan) ||
                 IEEE80211_IS_CHAN_11NG(vap->iv_bsschan) ||
                 IEEE80211_IS_CHAN_11AXG(vap->iv_bsschan)) ) ||
            vap->iv_opmode == IEEE80211_M_BTAMP) { /* No IBSS Support */

        if (ieee80211_vap_erpupdate_is_set(vap) && bo->bo_erp) {
            ieee80211_add_erp(bo->bo_erp, ic);
            ieee80211_vap_erpupdate_clear(vap);
        }
    }
}

#if UMAC_SUPPORT_WNM
static int ieee80211_beacon_add_wnm_ie(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        struct ieee80211_ath_tim_ie *tie,
        wbuf_t wbuf,
        int *is_dtim,
        uint32_t nfmsq_mask)
{
    struct ieee80211vap *vap = ni->ni_vap;
    uint32_t fms_counter_mask = 0;
    uint8_t *fmsie = NULL;
    uint8_t fmsie_len = 0;

    /* Add WNM specific IEs (like FMS desc...), if supported */
    if (ieee80211_vap_wnm_is_set(vap) &&
            ieee80211_wnm_fms_is_set(vap->wnm) &&
            vap->iv_opmode == IEEE80211_M_HOSTAP &&
            (bo->bo_fms_desc) && (bo->bo_fms_trailer)) {
        ieee80211_wnm_setup_fmsdesc_ie(ni, *is_dtim, &fmsie, &fmsie_len,
                &fms_counter_mask);

        if (fmsie_len != bo->bo_fms_len) {
            uint8_t *new_fms_trailer = (bo->bo_fms_desc + fmsie_len);
            int trailer_adjust =  new_fms_trailer - bo->bo_fms_trailer;

            /* Copy up/down trailer */
            if(trailer_adjust > 0) {
                uint8_t *tempbuf;

                tempbuf = OS_MALLOC(vap->iv_ic->ic_osdev,
                        bo->bo_fms_trailerlen , GFP_KERNEL);
                if (tempbuf == NULL) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                            "%s: Unable to alloc FMS copy buf. Size=%d\n",
                            __func__, bo->bo_fms_trailerlen);
                    return -1;
                }

                OS_MEMCPY(tempbuf, bo->bo_fms_trailer, bo->bo_fms_trailerlen);
                OS_MEMCPY(new_fms_trailer, tempbuf, bo->bo_fms_trailerlen);
                OS_FREE(tempbuf);
            } else {
                OS_MEMCPY(new_fms_trailer, bo->bo_fms_trailer,
                        bo->bo_fms_trailerlen);
            }

            bo->bo_fms_trailer = new_fms_trailer;
            bo->bo_extcap += trailer_adjust;

            if (bo->bo_wme)
                bo->bo_wme += trailer_adjust;

            if (bo->bo_htinfo_vendor_specific)
                bo->bo_htinfo_vendor_specific += trailer_adjust;

            bo->bo_ath_caps += trailer_adjust;

            if (bo->bo_apriori_next_channel)
                bo->bo_apriori_next_channel += trailer_adjust;

            if (bo->bo_bwnss_map != NULL)
                bo->bo_bwnss_map += trailer_adjust;

            bo->bo_xr += trailer_adjust;
            bo->bo_appie_buf += trailer_adjust;

            if(bo->bo_whc_apinfo)
                bo->bo_whc_apinfo += trailer_adjust;

#if QCN_IE
            if(bo->bo_qcn_ie)
                bo->bo_qcn_ie += trailer_adjust;
#endif
#if QCN_ESP_IE
            if(bo->bo_esp_ie)
                bo->bo_esp_ie += trailer_adjust;
#endif

            if (bo->bo_ap_chan_rpt)
                bo->bo_ap_chan_rpt += trailer_adjust;
            if (bo->bo_rnr)
                bo->bo_rnr += trailer_adjust;
            if (bo->bo_mbo_cap)
                bo->bo_mbo_cap += trailer_adjust;

            if (fmsie_len > bo->bo_fms_len)
                wbuf_append(wbuf, fmsie_len - bo->bo_fms_len);
            else
                wbuf_trim(wbuf, bo->bo_fms_len - fmsie_len);

            bo->bo_fms_len = fmsie_len;
        }

        if (fmsie_len &&  (bo->bo_fms_desc) && (bo->bo_fms_trailer)) {
            OS_MEMCPY(bo->bo_fms_desc, fmsie, fmsie_len);
            bo->bo_fms_trailer = bo->bo_fms_desc + fmsie_len;
            bo->bo_fms_len = fmsie_len;
        }

        if (tie != NULL) {
            /* Update state for buffered multicast frames on DTIM */
            if (nfmsq_mask & fms_counter_mask)
                tie->tim_bitctl |= 1;
        }
    }

    return 0;
}
#endif /* UMAC_SUPPORT_WNM */

static void ieee80211_add_apriori_next_chan(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo)
{
    struct ieee80211com *ic = ni->ni_ic;

    if(IEEE80211_IS_CSH_OPT_APRIORI_NEXT_CHANNEL_ENABLED(ic) &&
            IEEE80211_IS_CHAN_DFS(ic->ic_curchan)) {
        if(bo->bo_apriori_next_channel && ic->ic_tx_next_ch)
            ieee80211_add_next_channel(bo->bo_apriori_next_channel, ni, ic,
                    IEEE80211_FC0_SUBTYPE_BEACON);
    }
}

/* Add APP_IE buffer if app updated it */
static void ieee80211_beacon_add_app_ie(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed,
        bool *update_beacon_copy)
{
    uint8_t *frm = NULL;

#ifdef ATH_BEACON_DEFERRED_PROC
    IEEE80211_VAP_LOCK(vap);
#endif

    if (IEEE80211_VAP_IS_APPIE_UPDATE_ENABLED(vap)) {
        /* Adjust the buffer size if the size is changed */
        uint32_t newlen = vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length;

        /* Also add the length from the new App IE module */
        newlen += vap->iv_app_ie_list[IEEE80211_FRAME_TYPE_BEACON].total_ie_len;

        if (newlen != bo->bo_appie_buf_len) {
            int diff_len;

            diff_len = newlen - bo->bo_appie_buf_len;
            bo->bo_appie_buf_len = (u_int16_t) newlen;

            /* update the trailer lens */
            bo->bo_chanswitch_trailerlen += diff_len;
            bo->bo_tim_trailerlen += diff_len;
            bo->bo_secchanoffset_trailerlen += diff_len;

#if ATH_SUPPORT_IBSS_DFS
            bo->bo_ibssdfs_trailerlen += diff_len;
#endif

            if (diff_len > 0)
                wbuf_append(wbuf, diff_len);
            else
                wbuf_trim(wbuf, -(diff_len));

            *len_changed = 1;
        }

        if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length) {
            OS_MEMCPY(bo->bo_appie_buf,
                    vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].ie,
                    vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length);
        }

        /* Add the Application IE's */
        if (vap->iv_app_ie_list[IEEE80211_FRAME_TYPE_BEACON].total_ie_len) {
            frm = bo->bo_appie_buf +
                vap->iv_app_ie[IEEE80211_FRAME_TYPE_BEACON].length;
            frm = ieee80211_mlme_app_ie_append(vap, IEEE80211_FRAME_TYPE_BEACON,
                    frm);
        }

        IEEE80211_VAP_APPIE_UPDATE_DISABLE(vap);

        *update_beacon_copy = true;
    }

#ifdef ATH_BEACON_DEFERRED_PROC
    IEEE80211_VAP_UNLOCK(vap);
#endif
}

#if QCA_SUPPORT_SON
static int ieee80211_beacon_add_son_ie(
        struct ieee80211vap *vap,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed,
        bool *update_beacon_copy)
{
    uint8_t *frm = NULL;

    if (IEEE80211_VAP_IS_WDS_ENABLED(vap) &&
            son_vdev_fext_capablity(vap->vdev_obj,SON_CAP_GET,
                WLAN_VDEV_FEXT_SON_INFO_UPDATE) &&
            !son_vdev_map_capability_get(vap->vdev_obj, SON_MAP_CAPABILITY)) {
        uint16_t whcCaps = QCA_OUI_WHC_AP_INFO_CAP_WDS;
        uint16_t newlen;
        uint8_t *tempbuf = NULL;

        tempbuf = OS_MALLOC(vap->iv_ic->ic_osdev,
                sizeof(struct ieee80211_ie_whc_apinfo),
                GFP_KERNEL);
        if(tempbuf == NULL)
            return -1;

        /* SON mode requires WDS as a prereq */
        if (son_vdev_feat_capablity(vap->vdev_obj, SON_CAP_GET,
                    WLAN_VDEV_F_SON))
            whcCaps |= QCA_OUI_WHC_AP_INFO_CAP_SON;

        son_add_ap_info_ie(tempbuf, whcCaps, vap->vdev_obj, &newlen);

        if(newlen != bo->bo_whc_apinfo_len) {
            int diff_len = newlen - bo->bo_whc_apinfo_len;

            bo->bo_whc_apinfo_len = newlen;

            /* update the trailer lens */
            bo->bo_chanswitch_trailerlen += diff_len;
            bo->bo_tim_trailerlen += diff_len;
            bo->bo_secchanoffset_trailerlen += diff_len;

#if ATH_SUPPORT_IBSS_DFS
            bo->bo_ibssdfs_trailerlen += diff_len;
#endif

            if (diff_len > 0)
                wbuf_append(wbuf, diff_len);
            else
                wbuf_trim(wbuf, -(diff_len));

            *len_changed = 1;
        }

        if (bo->bo_whc_apinfo) {
            OS_MEMCPY(bo->bo_whc_apinfo, tempbuf, bo->bo_whc_apinfo_len);
            frm = bo->bo_whc_apinfo + bo->bo_whc_apinfo_len;
            son_vdev_fext_capablity(vap->vdev_obj,SON_CAP_CLEAR,
                    WLAN_VDEV_FEXT_SON_INFO_UPDATE);
        }

        OS_FREE(tempbuf);
        *update_beacon_copy = true;
    }

    return 0;
}
#endif

#if DBDC_REPEATER_SUPPORT
/* Add Extender IE */
static void ieee80211_beacon_add_extender_ie(
        struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int *len_changed,
        bool *update_beacon_copy)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct global_ic_list *ic_list = ic->ic_global_list;

    if (ic_list->same_ssid_support) {
       if (bo->bo_extender_ie) {
            /* Add the Extender IE */
           ieee80211_add_extender_ie(vap, IEEE80211_FRAME_TYPE_BEACON, bo->bo_extender_ie);
       } else {
           ieee80211_beacon_reinit(ni, bo, wbuf, len_changed, update_beacon_copy);
       }
    }
}
#endif

void
ieee80211_adjust_bos_for_bsscolor_change_ie(
        struct ieee80211_beacon_offsets *bo,
        uint8_t offset) {

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO, "%s>>", __func__);

    /* Update the pointers following this element and also trailer
     * length.
     */
    bo->bo_tim_trailerlen             += offset;
    bo->bo_chanswitch_trailerlen      += offset;
    bo->bo_vhtchnsw_trailerlen        += offset;
    bo->bo_secchanoffset_trailerlen   += offset;
#if ATH_SUPPORT_IBSS_DFS
    bo->bo_ibssdfs_trailerlen         += offset;
#endif
#if UMAC_SUPPORT_WNM
    bo->bo_fms_trailerlen             += offset;
#endif

    if (bo->bo_htinfo_pre_ana)
        bo->bo_htinfo_pre_ana += offset;

    if (bo->bo_wme)
        bo->bo_wme += offset;

    if (bo->bo_htinfo_vendor_specific)
        bo->bo_htinfo_vendor_specific += offset;

    if (bo->bo_bwnss_map)
        bo->bo_bwnss_map += offset;

    if (bo->bo_ath_caps)
        bo->bo_ath_caps += offset;

    if (bo->bo_apriori_next_channel)
        bo->bo_apriori_next_channel += offset;

    if (bo->bo_xr)
        bo->bo_xr += offset;

    if (bo->bo_appie_buf)
        bo->bo_appie_buf += offset;

    if(bo->bo_whc_apinfo)
        bo->bo_whc_apinfo += offset;

#if QCN_IE
    if(bo->bo_qcn_ie)
        bo->bo_qcn_ie += offset;
#endif

#if QCN_ESP_IE
    if(bo->bo_esp_ie)
        bo->bo_esp_ie += offset;
#endif

    if (bo->bo_ap_chan_rpt)
        bo->bo_ap_chan_rpt += offset;

    if (bo->bo_rnr)
        bo->bo_rnr += offset;

    if (bo->bo_mbo_cap)
        bo->bo_mbo_cap += offset;

#if UMAC_SUPPORT_WNM
    if (bo->bo_fms_desc)
        bo->bo_fms_desc += offset;

    if (bo->bo_fms_trailer)
        bo->bo_fms_trailer += offset;
#endif

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_INFO, "%s<<", __func__);
}

static int ieee80211_csa_interop_bss_is_desired(struct ieee80211vap *vap)
{
    struct ieee80211com *ic;
    int desired = 0;

    ic = vap->iv_ic;

    if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
        desired = 1;
        if (!vap->iv_csa_interop_bss)
            desired = 0;
    }

    return desired;
}

/*
 * Update the dynamic parts of a beacon frame based on the current state.
 */
#if UMAC_SUPPORT_WNM
int ieee80211_beacon_update(struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int mcast,
        u_int32_t nfmsq_mask)
#else
int ieee80211_beacon_update(struct ieee80211_node *ni,
        struct ieee80211_beacon_offsets *bo,
        wbuf_t wbuf,
        int mcast)
#endif
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int len_changed = 0;
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    bool update_beacon_copy = false;
    struct ieee80211_ath_tim_ie *tie = NULL;
    systime_t curr_time = OS_GET_TIMESTAMP();
    static systime_t prev_store_beacon_time;
    int retval;
#if UMAC_SUPPORT_WNM
    int is_dtim = 0;
#endif
    int interop_bss_desired;

    if((curr_time - prev_store_beacon_time) >=
            INTERVAL_STORE_BEACON * NUM_MILLISEC_PER_SEC){
        update_beacon_copy = true;
        prev_store_beacon_time = curr_time;
    }

#if QCN_IE
    /* If broadcast probe response feature is enabled and beacon offload is not enabled
     * then flag the current beacon as sent and calculate the next beacon timestamp.
     */
    if (vap->iv_bpr_enable && (!vap->iv_bcn_offload_enable)) {
       ieee80211_flag_beacon_sent(vap);
    }
#endif
    vap->iv_estimate_tbtt = ktime_to_ms(ktime_get());

    /* Update neighbor APs informations for AP Channel Report IE, RNR IE and MBO_OCE IE */
    if ((CONVERT_SYSTEM_TIME_TO_MS(curr_time) - vap->nbr_scan_ts) >=
        (vap->nbr_scan_period * NUM_MILLISEC_PER_SEC)) {

        if (vap->ap_chan_rpt_enable && !ieee80211_bg_scan_enabled(vap))
            ieee80211_update_ap_chan_rpt(vap);

        if (vap->rnr_enable && !ieee80211_bg_scan_enabled(vap))
            ieee80211_update_rnr(vap);
#if ATH_SUPPORT_MBO
        if (ieee80211_vap_oce_check(vap)) {
            ieee80211_update_non_oce_ap_presence (vap);
            if (IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan))
                ieee80211_update_11b_ap_presence (vap);
        }
#endif
        vap->nbr_scan_ts = CONVERT_SYSTEM_TIME_TO_MS(curr_time);
    }
#if QCN_ESP_IE
    if(ic->ic_esp_flag == 1){
        vap->iv_update_vendor_ie = 1;
        ic->ic_esp_flag = 0;
    }
#endif

    /* If Beacon Tx is suspended, then don't send this beacon */
    if (ieee80211_mlme_beacon_suspend_state(vap)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                "%s: skip Tx beacon during to suspend.\n", __func__);
        return -1;
    }

    /*
     * If vap is paused do not send any beacons to prevent transmitting
     * beacons on wrong channel.
     */
    if (ieee80211_vap_is_paused(vap))
        return -1;

    /*
     * Use the non-QoS sequence number space for BSS node
     * to avoid sw generated frame sequence the same as H/W generated frame,
     * the value lower than min_sw_seq is reserved for HW generated frame.
     */
    if ((ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] & IEEE80211_SEQ_MASK) <
            MIN_SW_SEQ)
        ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] = MIN_SW_SEQ;

    *(uint16_t *)&wh->i_seq[0] = htole16(
            ni->ni_txseqs[IEEE80211_NON_QOS_SEQ] << IEEE80211_SEQ_SEQ_SHIFT);
    ni->ni_txseqs[IEEE80211_NON_QOS_SEQ]++;

    interop_bss_desired = ieee80211_csa_interop_bss_is_desired(vap);

    vap->beacon_reinit_done = false;

    if (interop_bss_desired != vap->iv_csa_interop_bss_active) {
        qdf_info("csa interop bss %hhu -> %hhu",
                 vap->iv_csa_interop_bss_active, interop_bss_desired);

        vap->iv_csa_interop_bss_active = interop_bss_desired;
        ieee80211_beacon_reinit(ni, bo, wbuf, &len_changed, &update_beacon_copy);
    }

    ieee80211_beacon_check_and_reinit_beacon(ni, bo, wbuf, &len_changed,
            &update_beacon_copy);

    IEEE80211_CHAN_CHANGE_LOCK(ic);
    if (!IEEE80211_CHANCHANGE_STARTED_IS_SET(ic) &&
            (ic->ic_flags & IEEE80211_F_CHANSWITCH)) {
        IEEE80211_CHANCHANGE_STARTED_SET(ic);
        IEEE80211_CHANCHANGE_BY_BEACONUPDATE_SET(ic);
    }
    IEEE80211_CHAN_CHANGE_UNLOCK(ic);

    retval = ieee80211_change_channel(ni, &update_beacon_copy,
            &len_changed, wbuf, bo);
    if (!retval)
        return -1;

    /* Update cap info */
    ieee80211_update_capinfo(vap, bo);

    /* Add WME param */
    ieee80211_beacon_add_wme_param(vap, bo, &update_beacon_copy);

    /* Update power constraints */
    ieee80211_beacon_update_pwrcnstr(vap, bo);

    /* Update quiet param */
    ieee80211_quiet_beacon_update(vap, ic, bo);

    /* Update channel utilization information */
    ieee80211_update_chan_utilization(vap);

    /* Update bssload*/
    ieee80211_qbssload_beacon_update(vap, ni, bo);

    /* Add HT capability */
    ieee80211_beacon_add_htcap(ni, bo);

    /* Add VHT capability */
    ieee80211_beacon_add_vhtcap(ni, bo);

    /* Add HE BSS Color Change IE */
    ieee80211_beacon_add_bsscolor_change_ie(ni, bo, wbuf, &len_changed);

    /* Add HE capability */
    ieee80211_add_he_cap(ni, bo);

    /* Update MU-EDCA param */
    ieee80211_beacon_update_muedca_param(vap, bo, &update_beacon_copy);

    /* Update TIM */
    ieee80211_beacon_update_tim(ni, bo, &tie, wbuf, &len_changed, mcast,
            &is_dtim);

    ieee80211_beacon_add_chan_switch_ie(ni, bo, wbuf, &len_changed);

    /* Increment the TIM update beacon count to indicate change in SR IE */
    if(vap->iv_is_spatial_reuse_updated) {
        update_beacon_copy = true;
        vap->iv_is_spatial_reuse_updated = false;
    }
    /* Increment the TIM update beacon count to indicate inclusion of BCCA IE */
    if(vap->iv_bcca_ie_status == BCCA_START) {
        update_beacon_copy = true;
    }
    /* Increment the TIM update beacon count to indicate change in HEOP param */
    if(vap->iv_is_heop_param_updated) {
        update_beacon_copy = true;
        vap->iv_is_heop_param_updated = false;
    }
#if ATH_SUPPORT_IBSS_DFS
    retval = ieee80211_beacon_update_ibss_dfs(vap, bo, wbuf, &len_changed);
    if (retval == -1)
        return -1;
#endif

    /* Update ERP IE */
    ieee80211_beacon_erp_update(vap, bo);

    /* Update WNM IE */
#if UMAC_SUPPORT_WNM
    retval = ieee80211_beacon_add_wnm_ie(ni, bo, tie, wbuf,
            &is_dtim, nfmsq_mask) ;
    if (retval == -1)
        return -1;
#endif

    /* Update APRIORI next channel */
    ieee80211_add_apriori_next_chan(ni, bo);

    /* Add application IE */
    ieee80211_beacon_add_app_ie(vap, bo, wbuf, &len_changed,
            &update_beacon_copy);

    /* Add SON IE */
#if QCA_SUPPORT_SON
    retval = ieee80211_beacon_add_son_ie(vap, bo, wbuf, &len_changed,
            &update_beacon_copy);
    if (retval == -1)
        return -1;
#endif

#if DBDC_REPEATER_SUPPORT
    /* Update Extender IE */
    ieee80211_beacon_add_extender_ie(ni, bo, wbuf, &len_changed,
            &update_beacon_copy);
#endif
#if UMAC_SUPPORT_WNM
    if (update_beacon_copy) {
        ieee80211_wnm_tim_incr_checkbeacon(vap);
    }
#endif

    if (update_beacon_copy && ieee80211_vap_copy_beacon_is_set(vap)) {
        store_beacon_frame(vap, (uint8_t *)wbuf_header(wbuf),
                wbuf_get_pktlen(wbuf));
    }

    return len_changed;
}

int
wlan_copy_ap_beacon_frame(wlan_if_t vaphandle,
                          u_int32_t in_buf_size,
                          u_int32_t *required_buf_size,
                          void *buffer)
{
    struct ieee80211vap *vap = vaphandle;
    void *beacon_buf;

    *required_buf_size = 0;

    /* Make sure that this VAP is SoftAP */
    if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
        return EPERM ;
    }

    if (!vap->iv_beacon_copy_buf) {
        /* Error: no beacon buffer */
        return EPERM ;
    }

    if (in_buf_size < vap->iv_beacon_copy_len) {
        /* Input buffer too small */
        *required_buf_size = vap->iv_beacon_copy_len;
        return ENOMEM ;
    }
    *required_buf_size = vap->iv_beacon_copy_len;

    beacon_buf = (void *)vap->iv_beacon_copy_buf;

    OS_MEMCPY(buffer, beacon_buf, vap->iv_beacon_copy_len);

    return EOK;
}

void wlan_vdev_beacon_update(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (vap->iv_bcn_offload_enable &&
            ieee80211_is_vap_state_running(vap) &&
            (vap->iv_opmode == IEEE80211_M_HOSTAP) &&
            ic->ic_vdev_beacon_template_update) {
        ic->ic_vdev_beacon_template_update(vap);
    }

    return;
}
EXPORT_SYMBOL(wlan_vdev_beacon_update);

void wlan_pdev_beacon_update(struct ieee80211com *ic)
{
    struct ieee80211vap *vap;

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
        if (vap)
            wlan_vdev_beacon_update(vap);

    return;

}

void ieee80211_csa_interop_phy_update(struct ieee80211_node *ni, int rx_bw)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    int chan_bw;

    if (!ni) {
        return;
    }

    vap = ni->ni_vap;
    ic = vap->iv_ic;

    switch (rx_bw) {
        case IEEE80211_CWM_WIDTH20:
        case IEEE80211_CWM_WIDTH40:
        case IEEE80211_CWM_WIDTH80:
        case IEEE80211_CWM_WIDTH160:
            switch (ieee80211_get_chan_width(vap->iv_bsschan)) {
                case 5:
                case 10:
                case 20:
                    chan_bw = IEEE80211_CWM_WIDTH20;
                    break;
                case 40:
                    chan_bw = IEEE80211_CWM_WIDTH40;
                    break;
                case 80:
                    chan_bw = IEEE80211_CWM_WIDTH80;
                    break;
                case 160:
                    chan_bw = IEEE80211_CWM_WIDTH160;
                    break;
                default:
                    chan_bw = IEEE80211_CWM_WIDTH20;
                    break;
            }

            if (rx_bw <= ni->ni_chwidth)
                break;

            if (rx_bw > chan_bw)
                break;

            if (unlikely(WARN_ONCE((unlikely(!(ni->ni_flags & IEEE80211_NODE_HT)) &&
                                    unlikely(rx_bw >= IEEE80211_CWM_WIDTH40)),
                                   "%s: [%s, %pM] ignoring %d -> %d, !ht && cw>=40",
                                   __func__, vap->iv_netdev_name, ni->ni_macaddr,
                                   ni->ni_chwidth, rx_bw)))
                break;

            if (unlikely(WARN_ONCE((unlikely(!(ni->ni_flags & IEEE80211_NODE_VHT)) &&
                                    unlikely(rx_bw >= IEEE80211_CWM_WIDTH80)),
                                   "%s: [%s, %pM] ignoring %d -> %d, !vht && cw>=80",
                                   __func__, vap->iv_netdev_name, ni->ni_macaddr,
                                   ni->ni_chwidth, rx_bw)))
                break;

            if (unlikely(WARN_ONCE((unlikely(!ni->ni_160bw_requested) &&
                                    unlikely(rx_bw >= IEEE80211_CWM_WIDTH160)),
                                   "%s: [%s, %pM] ignoring %d -> %d, !160assoc && cw>=160",
                                   __func__, vap->iv_netdev_name, ni->ni_macaddr,
                                   ni->ni_chwidth, rx_bw)))
                break;

            qdf_debug("[%s, %pM] upgrading CW %d -> %d (chan_bw=%d)",
                     vap->iv_netdev_name, ni->ni_macaddr,
                     ni->ni_chwidth, rx_bw, chan_bw);

            ni->ni_chwidth = rx_bw;
            ic->ic_chwidth_change(ni);
            break;
        case -1:
            qdf_debug("[%s, %pM] downgrading CW %d -> %d",
                     vap->iv_netdev_name, ni->ni_macaddr, ni->ni_chwidth,
                     IEEE80211_CWM_WIDTH20);

            ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            ic->ic_chwidth_change(ni);
            break;
        default:
            qdf_debug("[%s, %pM] unsupported CW %d -> %d, ignoring",
                     vap->iv_netdev_name, ni->ni_macaddr,
                     ni->ni_chwidth, rx_bw);
            break;
    }
}

void ieee80211_csa_interop_update(void *ctrl_pdev, enum WDI_EVENT event,
                                  void *buf, uint16_t id, uint32_t type)
{
    qdf_nbuf_t nbuf;
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211com *ic;
    struct cdp_rx_indication_ppdu *cdp_rx_ppdu;
    uint32_t bw;
    struct ieee80211_node *ni;

    nbuf = buf;
    if (!nbuf) {
        qdf_err("nbuf is null");
        return;
    }

    pdev = (struct wlan_objmgr_pdev *)ctrl_pdev;
    if (!pdev) {
        qdf_err("pdev is null");
        return;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    cdp_rx_ppdu = (struct cdp_rx_indication_ppdu *)qdf_nbuf_data(nbuf);
    bw = cdp_rx_ppdu->u.bw;

    ni = ieee80211_find_node(&ic->ic_sta, cdp_rx_ppdu->mac_addr);
    if (!ni) {
        qdf_err("ni is null");
        return;
    }

    ieee80211_csa_interop_phy_update(ni, bw);
    ieee80211_free_node(ni);
}
qdf_export_symbol(ieee80211_csa_interop_update);
