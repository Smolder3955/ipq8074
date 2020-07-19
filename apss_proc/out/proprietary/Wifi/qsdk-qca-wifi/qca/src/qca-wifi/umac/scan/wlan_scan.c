/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <wlan_cmn.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_scan_ucfg_api.h>
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_proto.h>
#include <ieee80211_api.h>
#include <wlan_cmn_ieee80211.h>
#include <ol_if_athvar.h>
#include <wlan_utility.h>

/**
 * convert_ieee_ssid_to_wlan_ssid() - Function to convert ieee_ssid struct to wlan_ssid struct
 *
 * @param num_ssid: number of ssids present in ieee_ssid list
 * @param ieeessid: ieee_ssid list
 * @param wlanssid: wlan_ssid out param list
 *
 * @return QDF_STATUS_SUCCESS for success  or QDF_STATUS_E_INVAL in case of failure.
 */
QDF_STATUS
convert_ieee_ssid_to_wlan_ssid(uint32_t num_ssid, ieee80211_ssid *ieeessid,
        struct wlan_ssid *wlanssid)
{
    if (!num_ssid)
        return QDF_STATUS_SUCCESS;

    if (!ieeessid || !wlanssid)
        return QDF_STATUS_E_INVAL;

    while (num_ssid) {
        wlanssid->length = ieeessid->len;
        if (wlanssid->length > sizeof(wlanssid->ssid))
            wlanssid->length = sizeof(wlanssid->ssid);
        qdf_mem_copy(wlanssid->ssid, ieeessid->ssid, wlanssid->length);
        ++wlanssid;
        ++ieeessid;
        --num_ssid;
    }
    return QDF_STATUS_SUCCESS;
}

struct wlan_ssid *
convert_ieee_ssid_to_wlan_ssid_inplace(uint32_t num_ssid, ieee80211_ssid *ieeessid)
{
    ieee80211_ssid tmp_ssid = {0};
    struct wlan_ssid *wlanssid = (struct wlan_ssid *)ieeessid;
    struct wlan_ssid *start = wlanssid;

    if (!num_ssid)
        return NULL;

    if (!wlanssid)
        return NULL;

    while (num_ssid) {
        tmp_ssid = *ieeessid;
        wlanssid->length = tmp_ssid.len;
        if (wlanssid->length > sizeof(wlanssid->ssid))
            wlanssid->length = sizeof(wlanssid->ssid);
        qdf_mem_copy(wlanssid->ssid, tmp_ssid.ssid, wlanssid->length);
        ++wlanssid;
        ++ieeessid;
        --num_ssid;
    }
    return start;
}

QDF_STATUS
wlan_update_scan_params(wlan_if_t vap,
        struct scan_start_request *scan_params,
        enum ieee80211_opmode opmode,
        bool                  active_scan_flag,
        bool                  high_priority_flag,
        bool                  connected_flag,
        bool                  external_scan_flag,
        uint32_t             num_ssid,
        ieee80211_ssid        *ssid_list,
        int                   peer_count)
{
#define DEFAULT_SCAN_MIN_DWELL_TIME_ACTIVE 	105
#define DEFAULT_SCAN_MIN_DWELL_TIME_PASSIVE	130
#define DEFAULT_SCAN_MAX_REST_TIME 		500
#define DEFAULT_SCAN_MIN_REST_TIME_IBSS 	250
#define DEFAULT_SCAN_MIN_REST_TIME_INFRA 	50
#define DEFAULT_NOT_CONNECTED_DWELL_TIME_FACTOR 2

    int i = 0;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    uint8_t *tmp_bssid = NULL;
    uint32_t nbssid = 0;
    struct wlan_ssid *wlan_ssid_list = NULL;
    struct ieee80211_aplist_config *apconfig = NULL;
    struct qdf_mac_addr
        bssid_list[IEEE80211_N(scan_params->scan_req.bssid_list)];

    if (!vap || !scan_params || num_ssid >= IEEE80211_SCAN_MAX_SSID) {
        return QDF_STATUS_E_INVAL;
    }

    apconfig = ieee80211_vap_get_aplist_config(vap);
    /* ucfg_scan_init_default_params() initializes scan parameters
     * asuming all the flags passed to wlan_set_default_scan_parameters() ZERO.
     * Any adjustment of parameters has to be done after calling ucfg_scan_init_default_params()
     * default opmode is taken as INFRA
     */
    ucfg_scan_init_default_params(vap->vdev_obj, scan_params);

    /* init legacy DA scan fields */
    scan_params->legacy_params.min_dwell_passive = DEFAULT_SCAN_MIN_DWELL_TIME_PASSIVE;
    /* update scan parameters as required */
    if (active_scan_flag) {
        scan_params->legacy_params.min_dwell_active =
            DEFAULT_SCAN_MIN_DWELL_TIME_ACTIVE;
        scan_params->scan_req.scan_f_passive = false;
    } else {
        scan_params->legacy_params.min_dwell_active =
            DEFAULT_SCAN_MIN_DWELL_TIME_PASSIVE;
        scan_params->scan_req.scan_f_passive = true;
    }

    if (high_priority_flag) {
        scan_params->scan_req.max_rest_time = DEFAULT_SCAN_MAX_REST_TIME;
        scan_params->scan_req.scan_f_forced = true;
    }

    if (opmode == IEEE80211_M_IBSS) {
        scan_params->scan_req.min_rest_time = DEFAULT_SCAN_MIN_REST_TIME_IBSS;
        if (peer_count > 0) {
            scan_params->legacy_params.scan_type = SCAN_TYPE_BACKGROUND;
        } else {
            scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;
        }
    } else {
        scan_params->scan_req.min_rest_time = DEFAULT_SCAN_MIN_REST_TIME_INFRA;
        if (connected_flag) {
            scan_params->legacy_params.scan_type = SCAN_TYPE_BACKGROUND;
        } else {
            scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;
        }
    }

#if QCA_LTEU_SUPPORT
    scan_params->scan_req.scan_ev_gpio_timeout = true;
#endif

    /*
     * Validate time before sending second probe request. A value of 0 is valid
     * and means a single probe request must be transmitted.
     */
    ASSERT(scan_params->scan_req.repeat_probe_time <
            scan_params->scan_req.min_dwell_time_active);

    /* Double the dwell time if not connected */
    if (!connected_flag)
    {
        scan_params->legacy_params.min_dwell_active *=
            DEFAULT_NOT_CONNECTED_DWELL_TIME_FACTOR;
    }

    wlan_ssid_list = convert_ieee_ssid_to_wlan_ssid_inplace(num_ssid, ssid_list);

    status = ucfg_scan_init_ssid_params(scan_params, num_ssid, wlan_ssid_list);
    if (status != QDF_STATUS_SUCCESS) {
        return status;
    }

    nbssid = ieee80211_aplist_get_desired_bssid_count(apconfig);
    if (nbssid > IEEE80211_N(scan_params->scan_req.bssid_list)) {
        nbssid = IEEE80211_N(scan_params->scan_req.bssid_list);
    }

    for (i = 0; i < nbssid; i++) {
        ieee80211_aplist_get_desired_bssid(apconfig, i, &tmp_bssid);
        qdf_mem_copy(&bssid_list[i].bytes[0], tmp_bssid, sizeof(bssid_list[0]));
    }

    status = ucfg_scan_init_bssid_params(scan_params, nbssid, &bssid_list[0]);
    if (status != QDF_STATUS_SUCCESS) {
        return status;
    }

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_ucfg_scan_start(wlan_if_t vap, struct scan_start_request *scan_params,
        wlan_scan_requester requestor_id, enum scan_priority priority,
        wlan_scan_id *scan_id, uint32_t optie_len, uint8_t *optie_data)
{
#define MAX_HT_IE_LEN   32
#define MAX_VHT_IE_LEN  32

    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211_rateset rs;
    uint8_t *ie_end = NULL;
    uint8_t *optie = NULL;
    bool cck_rates = false;
    bool is_connecting = false;
    int i;

    /* vdev, scan params and requester id must be valid */
    if (!vap || !scan_params || !requestor_id) {
        if (scan_params) {
            qdf_mem_free(scan_params);
        }
        return QDF_STATUS_E_INVAL;
    }

    ic = vap->iv_ic;
    if (!ic) {
        qdf_mem_free(scan_params);
        return QDF_STATUS_E_INVAL;
    }

    if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
        qdf_mem_free(scan_params);
        qdf_info("CSA in progress. Discard scan");
        return QDF_STATUS_E_BUSY;
    }

    if (ic->recovery_in_progress) {
        qdf_mem_free(scan_params);
        qdf_info("TA! Recovery in progress. Discard scan");
        return QDF_STATUS_E_BUSY;
    }

    /* If connection is in progress, do not start scan */
    is_connecting = ieee80211_vap_is_connecting(vap);
    if (is_connecting) {
        qdf_mem_free(scan_params);
        scan_info("%s: vap: 0x%pK is connecting, return BUSY", __func__, vap);
        return QDF_STATUS_E_BUSY;
    }

    /* Enable CCK rates if vaps operational rates contain CCK */
    qdf_mem_zero(&rs, sizeof(rs));
    wlan_get_operational_rates(vap,IEEE80211_MODE_11G, rs.rs_rates, IEEE80211_RATE_MAXSIZE, &i);
    rs.rs_nrates = i;
    for (i = 0; i < rs.rs_nrates; i++) {
        if (rs.rs_rates[i] == 0x2 || rs.rs_rates[i] == 0x4 ||
                rs.rs_rates[i] == 0xb || rs.rs_rates[i] == 0x16) {
             cck_rates = true;
        }
    }
    scan_params->scan_req.scan_f_cck_rates = cck_rates;

    /* Always enable OFDM rates */
    scan_params->scan_req.scan_f_ofdm_rates = true;

    vdev = vap->vdev_obj;
    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        scan_info("unable to get reference");
        qdf_mem_free(scan_params);
        return status;
    }

    /* As WIN driver doesn't support per vdev HT/VHT IE config WMI command,
     * add HT/VHT IE to optie. Once per VAP HT/VHT IE config command
     * is available, adding HT/VHT IE to optie should be removed and WMI
     * command should be invoked as and when HT/VHT IE changes.
     */

    /* Allocate memory big enough to hold optional/extra IE,
     * HT cap IE and VHT cap IE
     */
    optie = qdf_mem_malloc(optie_len + MAX_HT_IE_LEN + MAX_VHT_IE_LEN);
    if (!optie) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        scan_info("unable to allocate optie");
        qdf_mem_free(scan_params);
        return QDF_STATUS_E_NOMEM;
    }

    if (vap->iv_bss) {
        /* Add HT cap ie */
        ie_end = ieee80211_add_htcap(optie, vap->iv_bss,
                        IEEE80211_FC0_SUBTYPE_PROBE_REQ);

        /* Add VHT cap ie */
        ie_end = ieee80211_add_vhtcap(ie_end, vap->iv_bss, ic,
                        IEEE80211_FC0_SUBTYPE_PROBE_REQ, NULL, NULL);
    }

    /* Add opt IE */
    if (optie_len && optie_data) {
        qdf_mem_copy(ie_end, optie_data, optie_len);
        ie_end += optie_len;
    }

    scan_params->scan_req.extraie.ptr = optie;
    scan_params->scan_req.extraie.len = (uint32_t) (ie_end - optie);

    psoc = wlan_vdev_get_psoc(vdev);

    scan_params->scan_req.scan_id = *scan_id = ucfg_scan_get_scan_id(psoc);
    scan_params->scan_req.scan_req_id = requestor_id;
    scan_params->scan_req.scan_priority = priority;
    scan_params->vdev = vdev;

    if (!(scan_params->scan_req.chan_list.num_chan)) {
        scan_params->scan_req.scan_f_half_rate = false;
        scan_params->scan_req.scan_f_quarter_rate = false;
    }

    /* start scan */
    status = ucfg_scan_start(scan_params);

    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);

    return status;
}

QDF_STATUS
wlan_ucfg_scan_cancel(wlan_if_t vap, wlan_scan_requester requester_id,
        wlan_scan_id scan_id, enum scan_cancel_req_type type, bool sync)
{
    struct scan_cancel_request *cancel_req = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct wlan_objmgr_vdev *vdev = NULL;
    bool is_scan_active = false;

    if (!vap || !vap->vdev_obj) {
        return QDF_STATUS_E_INVAL;
    }
    vdev = vap->vdev_obj;

    /* connection and assoc state machines expect scan cancelled
     * event when ever wlan_ucfg_scan_cancel returns success.
     * let the caller know if no scans are in progress.
     */
    if (type == WLAN_SCAN_CANCEL_PDEV_ALL) {
        is_scan_active = wlan_scan_in_progress(vap);
    } else {
        is_scan_active = wlan_vdev_scan_in_progress(vdev);
    }

    if (!is_scan_active) {
        return QDF_STATUS_E_ALREADY;
    }

    cancel_req = qdf_mem_malloc(sizeof(*cancel_req));
    if (!cancel_req) {
        qdf_print("%s: unable to allocate memory."
                " scan_cancel for id: %d, req: %d, type: %d failed",
                __func__, scan_id, requester_id, type);
        return QDF_STATUS_E_NOMEM;
    }

    wlan_objmgr_vdev_get_ref(vdev, WLAN_OSIF_SCAN_ID);

    cancel_req->vdev = vdev;
    cancel_req->cancel_req.requester = requester_id;
    cancel_req->cancel_req.scan_id = scan_id;
    cancel_req->cancel_req.req_type = type;

    cancel_req->cancel_req.vdev_id = wlan_vdev_get_id(vdev);
    pdev = wlan_vdev_get_pdev(vdev);

    cancel_req->cancel_req.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

    if (sync) {
        status = ucfg_scan_cancel_sync(cancel_req);
    } else {
        status = ucfg_scan_cancel(cancel_req);
    }

    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);

    return status;
}


struct ieee80211_ath_channel *
wlan_find_full_channel(struct ieee80211com *ic, uint32_t freq)
{
    struct ieee80211_ath_channel *c = NULL;
    c = NULL;
#define IEEE80211_2GHZ_FREQUENCY_THRESHOLD    3000
    if (freq < IEEE80211_2GHZ_FREQUENCY_THRESHOLD) {
        if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT20)) {
            c = ieee80211_find_channel(ic, freq, 0, IEEE80211_CHAN_11NG_HT20);
        }
        if (c == NULL) {
            c = ieee80211_find_channel(ic, freq, 0, IEEE80211_CHAN_G);
        }
        if (c == NULL) {
            c = ieee80211_find_channel(ic, freq, 0, IEEE80211_CHAN_PUREG);
        }
        if (c == NULL) {
            c = ieee80211_find_channel(ic, freq, 0, IEEE80211_CHAN_B);
        }
    } else {
        if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT20)) {
            c = ieee80211_find_channel(ic, freq, 0, IEEE80211_CHAN_11NA_HT20);
        }
        if (c == NULL) {
            u_int32_t halfquarter = ic->ic_chanbwflag & (IEEE80211_CHAN_HALF | IEEE80211_CHAN_QUARTER);
            c = ieee80211_find_channel(ic, freq, 0, IEEE80211_CHAN_A | halfquarter);
        }
    }
    return c;
#undef IEEE80211_2GHZ_FREQUENCY_THRESHOLD
}

enum wlan_phymode
wlan_scan_get_intersected_phymode(struct ieee80211com *ic,
        struct scan_cache_entry *scan_entry)
{
    struct channel_info *se_chan = util_scan_entry_channel(scan_entry);
    uint8_t *rates = util_scan_entry_rates(scan_entry);
    uint8_t *xrates = util_scan_entry_xrates(scan_entry);
    struct ieee80211_ie_htcap_cmn *htcap =
        (struct ieee80211_ie_htcap_cmn *) util_scan_entry_htcap(scan_entry);
    struct ieee80211_ie_htinfo_cmn *htinfo =
        (struct ieee80211_ie_htinfo_cmn *) util_scan_entry_htinfo(scan_entry);
    struct ieee80211_ie_vhtcap *vhtcap =
        (struct ieee80211_ie_vhtcap *) util_scan_entry_vhtcap(scan_entry);
    struct ieee80211_ie_vhtop  *vhtop =
        (struct ieee80211_ie_vhtop  *) util_scan_entry_vhtop(scan_entry);
    struct ieee80211_ie_hecap *hecap =
        (struct ieee80211_ie_hecap *) util_scan_entry_hecap(scan_entry);
    struct ieee80211_ie_heop *heop =
        (struct ieee80211_ie_heop *) util_scan_entry_heop(scan_entry);
    struct ieee80211_ath_channel *bcn_chan =
        wlan_find_full_channel(ic, ieee80211_ieee2mhz(ic, se_chan->chan_idx, 0));

    if (!bcn_chan) {
        bcn_chan = ic->ic_curchan;
    }

    return ieee80211_get_phy_type(ic, rates, xrates, htcap,
            htinfo, vhtcap, vhtop, hecap, heop, bcn_chan);
}

wlan_chan_t
wlan_scan_get_intersected_channel(struct ieee80211com *ic,
        struct scan_cache_entry *scan_entry)
{
    wlan_chan_t chan = NULL;
    uint32_t halfquarter = 0, vhtcapinfo = 0;
    struct ieee80211_ie_vhtop *ap_vhtop = NULL;
    struct ieee80211_ie_vhtcap *vhtcap = NULL;
    struct ieee80211_ie_htinfo_cmn *htinfo = NULL;
    uint8_t vht_op_ch_freq_seg2 = 0;
    uint8_t bcn_chan_idx = util_scan_entry_channel(scan_entry)->chan_idx;
    enum ieee80211_phymode ap_phymode = util_scan_entry_phymode(scan_entry);

    struct ieee80211_ath_channel *bcn_recv_chan =
        wlan_find_full_channel(ic, ieee80211_ieee2mhz(ic, bcn_chan_idx, 0));

    if (!bcn_recv_chan) {
        bcn_recv_chan = ic->ic_curchan;
    }

    /*
     * Assign the right channel.
     * This is necessary for two reasons:
     *     -update channel flags which may have changed even if the channel
     *      number did not change
     *     -we may have received a beacon from an adjacent channel which is not
     *      supported by our regulatory domain. In this case we want
     *      scan_entry_parameters->chan to be set to NULL so that we know it is
     *      an invalid channel.
     */

    vhtcap = (struct ieee80211_ie_vhtcap *) util_scan_entry_vhtcap(scan_entry);
    htinfo = (struct ieee80211_ie_htinfo_cmn *) util_scan_entry_htinfo(scan_entry);
    ap_vhtop = (struct ieee80211_ie_vhtop *) util_scan_entry_vhtop(scan_entry);
    halfquarter = bcn_recv_chan->ic_flags &
        (IEEE80211_CHAN_HALF | IEEE80211_CHAN_QUARTER);

    if (vhtcap)
        vhtcapinfo = le32toh(vhtcap->vht_cap_info);

    if (ap_vhtop) {
        if (htinfo) {
            if ((ap_phymode == IEEE80211_MODE_11AXA_HE80_80 ||
                 ap_phymode == IEEE80211_MODE_11AC_VHT80_80) &&
                (ic->ic_ext_nss_capable && peer_ext_nss_capable(vhtcap)
                && extnss_80p80_validate_and_seg2_indicate(
                    (&vhtcapinfo), ap_vhtop, htinfo))) {
                vht_op_ch_freq_seg2 =
                    retrieve_seg2_for_extnss_80p80(
                        (&vhtcapinfo), ap_vhtop, htinfo);
            }
        } else if (IS_REVSIG_VHT160(ap_vhtop)) {
            vht_op_ch_freq_seg2 = 0;
        } else {
            vht_op_ch_freq_seg2 = ap_vhtop->vht_op_ch_freq_seg2;
        }

        chan = ieee80211_find_dot11_channel(ic, bcn_chan_idx,
                vht_op_ch_freq_seg2, ap_phymode | halfquarter);

        if((!chan) && ic->ic_emiwar_80p80 && \
                (ap_phymode == IEEE80211_MODE_11AC_VHT80_80)) {
            /*
             * When EMIWAR is FC1>FC2 Some 80_80 MHz channel
             * combination will not be available
             * When EMIWAR is BandEdge channle 149,153,157,161 with
             * secondary Frq 42 80_80 Mhz channel combination will
             * not be available
             * In which case STA need to fall back to 80MHz for
             * Optimal purpose instead of 11NA_HT20
             */
            chan = ieee80211_find_dot11_channel(ic, bcn_chan_idx,
                    vht_op_ch_freq_seg2,
                    IEEE80211_MODE_11AC_VHT80 | halfquarter);
        }
    } else {
        chan = ieee80211_find_dot11_channel(ic, bcn_chan_idx, 0,
                ap_phymode | halfquarter);
        if((!chan) && ic->ic_emiwar_80p80 && \
                (ap_phymode == IEEE80211_MODE_11AC_VHT80_80)) {
            /*
             * When EMIWAR is FC1>FC2 Some 80_80 MHz channel
             * combination will not be available
             * In which case STA need to fall back to 80MHz
             * for Optimal purpose instead of 11NA_HT20
             */
            chan = ieee80211_find_dot11_channel(ic, bcn_chan_idx, 0,
                    IEEE80211_MODE_11AC_VHT80 | halfquarter);
        }
    }

    /*
     * The most common reasons why ieee80211_find_dot11_channel can't find
     * a channel are:
     *     -Channel is not allowed for the current RegDomain
     *     -Channel flags do not match
     *
     * For the second case, *if* there beacon was transmitted in the
     * current channel (channel_mismatch is false), then it is OK to use
     * the current IC channel as the beacon channel.
     * The underlying assumption is that the Station will *not* scan
     * channels not allowed by the current regulatory domain/11d settings.
     */
    if (!chan) {
        chan = bcn_recv_chan;
    }

    return chan;
}

void wlan_scan_cache_update_callback(struct wlan_objmgr_pdev *pdev,
        struct scan_cache_entry* scan_entry)
{
    struct ieee80211com *ic = NULL;
    wlan_chan_t new_chan = NULL;
    wlan_chan_t prev_chan = NULL;
    struct channel_info *se_chan = util_scan_entry_channel(scan_entry);

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
        qdf_err("%s: ic is NULL", __func__);
        return;
    }
    scan_entry->phy_mode = wlan_scan_get_intersected_phymode(ic, scan_entry);
    new_chan = wlan_scan_get_intersected_channel(ic, scan_entry);

    prev_chan = se_chan->priv;
    if ((prev_chan == NULL) ||
            (wlan_channel_flags(prev_chan) != wlan_channel_flags(new_chan))) {
        se_chan->priv = new_chan;
    }

    /*
     * Process the beacons to derive bss-color from HE-OP params and build
     * the list of colors of the neighboring bsss so that we can use this
     * list at the time of color-selection and to detect color collisions
     */
    ieee80211_process_beacon_for_bsscolor_cache_update(ic, scan_entry);

    return;
}

int
wlan_scan_entry_rsncaps(wlan_if_t vaphandle,
        struct scan_cache_entry *scan_entry, u_int16_t *rsncaps)
{
    uint8_t                     *rsn_ie;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    struct wlan_crypto_params crypto_params;
#else
    struct ieee80211_rsnparms    rsn_parms;
#endif

    rsn_ie = util_scan_entry_rsn(scan_entry);
    if (rsn_ie == NULL)
        return -EIO;

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if (wlan_crypto_rsnie_check(&crypto_params, rsn_ie) != QDF_STATUS_SUCCESS) {
#else
    if (ieee80211_parse_rsn(vaphandle, rsn_ie, &rsn_parms) != IEEE80211_STATUS_SUCCESS) {
#endif
        return -EIO;
    }

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    *rsncaps = crypto_params.rsn_caps;
#else
    *rsncaps = rsn_parms.rsn_caps;
#endif
    return EOK;
}

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
int
wlan_scan_entry_rsnparams(wlan_if_t vaphandle, struct scan_cache_entry *scan_entry,
        struct wlan_crypto_params *crypto_params)
{
    uint8_t                     *rsn_ie;

    rsn_ie = util_scan_entry_rsn(scan_entry);
    if (rsn_ie == NULL)
        return -EIO;
    if (wlan_crypto_rsnie_check(crypto_params, rsn_ie) != QDF_STATUS_SUCCESS) {
        return -EIO;
    }

    return EOK;

    
}
#else
int
wlan_scan_entry_rsnparams(wlan_if_t vaphandle, struct scan_cache_entry *scan_entry,
        struct ieee80211_rsnparms *rsnparams)
{
    uint8_t                     *rsn_ie;

    rsn_ie = util_scan_entry_rsn(scan_entry);
    if (rsn_ie == NULL)
        return -EIO;

    if (ieee80211_parse_rsn(vaphandle, rsn_ie, rsnparams) !=
            IEEE80211_STATUS_SUCCESS) {
        return -EIO;
    }

    return EOK;
}
#endif


static inline QDF_STATUS
scan_db_macaddr_iterate(void *arg, struct scan_cache_entry *scan_entry)
{
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct macaddr_iterate_arg *iterate_arg = arg;

    if (IEEE80211_ADDR_EQ(&(iterate_arg->macaddr.bytes[0]),
                util_scan_entry_macaddr(scan_entry))) {
        /* match found */
        status = (*iterate_arg->func)(iterate_arg->arg, scan_entry);
    }
    return status;
}

QDF_STATUS
util_wlan_scan_db_iterate_macaddr(wlan_if_t vap, uint8_t *macaddr,
        scan_iterator_func func, void *arg)
{
    struct macaddr_iterate_arg iterate_arg;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    iterate_arg.func = func;
    iterate_arg.arg = arg;
    qdf_mem_copy(iterate_arg.macaddr.bytes, macaddr, QDF_MAC_ADDR_SIZE);

    status = ucfg_scan_db_iterate(wlan_vap_get_pdev(vap),
            scan_db_macaddr_iterate, &iterate_arg);

    return status;
}

static const u_int16_t default_scan_order[] = {
    /* 2.4Ghz ch: 1,6,11,7,13 */
    2412, 2437, 2462, 2442, 2472,
    /* 8 FCC channel: 52, 56, 60, 64, 36, 40, 44, 48 */
    5260, 5280, 5300, 5320, 5180, 5200, 5220, 5240,
    /* 2.4Ghz ch: 2,3,4,5,8,9,10,12 */
    2417, 2422, 2427, 2432, 2447, 2452, 2457, 2467,
    /* 2.4Ghz ch: 14 */
    2484,
    /* 6 FCC channel: 144, 149, 153, 157, 161, 165 */
    5720, 5745, 5765, 5785, 5805, 5825,
    /* 11 ETSI channel: 100,104,108,112,116,120,124,128,132,136,140 */
    5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680, 5700,
    /* Added Korean channels 2312-2372 */
    2312, 2317, 2322, 2327, 2332, 2337, 2342, 2347, 2352, 2357, 2362, 2367,
    2372 ,
    /* Added Japan channels in 4.9/5.0 spectrum */
    5040, 5060, 5080, 4920, 4940, 4960, 4980,
    /* FCC4 channels */
    4950, 4955, 4965, 4970, 4975, 4985,
    /* Added MKK2 half-rates */
    4915, 4920, 4925, 4935, 4940, 4945, 5035, 5040, 5045, 5055,
    /* Added FCC4 quarter-rates */
    4942, 4947, 4952, 4957, 4962, 4967, 4972, 4977, 4982, 4987,
    /* Add MKK quarter-rates */
    4912, 4917, 4922, 4927, 4932, 4937, 5032, 5037, 5042, 5047, 5052, 5057,
};


static inline struct wlan_lmac_if_scan_tx_ops *
wlan_get_scan_tx_ops(struct wlan_objmgr_psoc *psoc)
{
    return &(psoc->soc_cb.tx_ops.scan);
}

/*
 * get all the channels to scan .
 * can be called whenever the set of supported channels are changed.
 * send the updated channel list to target.
 */
QDF_STATUS
wlan_scan_update_channel_list(struct ieee80211com *ic)
{
#define IEEE80211_2GHZ_FREQUENCY_THRESHOLD    3000  /* in kHz */
    struct ieee80211_ath_channel  *c = NULL;
    struct ieee80211_ath_channel  *temp_chan = NULL;
    u_int16_t *scan_order = (u_int16_t *)default_scan_order;
    u_int32_t scan_order_size = IEEE80211_N(default_scan_order);
    int i = 0;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct scan_chan_list_params *param;
    struct ieee80211_ath_channel **all_chans = NULL;
    struct wlan_lmac_if_scan_tx_ops *scan_tx_ops = NULL;
    uint32_t frag_list_bytes = 0;
    struct scan_chan_list_params *frag_list = NULL;
    uint16_t nallchans = 0; /* Number of chan's to scan */
    uint16_t send_idx = 0;

    param = (struct scan_chan_list_params *)
        qdf_mem_malloc((sizeof(struct scan_chan_list_params) +
                    ((MAX_CHANS - 1) * sizeof(struct channel_param))));
    if (!param) {
        qdf_print("%s Unable to create memory to hold channel list param,"
                " Dropping scan chan list update", __func__);
        return QDF_STATUS_E_NOMEM;
    }

    wlan_update_current_mode_caps(ic);

    all_chans = (struct ieee80211_ath_channel **)
        qdf_mem_malloc(sizeof(struct ieee80211_ath_channel*) * IEEE80211_CHAN_MAX);
    if (!all_chans) {
        qdf_mem_free(param);
        qdf_print("%s Unable to create memory to hold all channels,"
                " Dropping scan chan list update", __func__);
        return QDF_STATUS_E_NOMEM;
    }

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);

    nallchans = 0;

    if (ic->ic_custom_scan_order_size > 0) {
        printk("Using custom scan order\n"),
            scan_order = ic->ic_custom_scan_order;
        scan_order_size = ic->ic_custom_scan_order_size;
    }

    for (i = 0; i < scan_order_size; ++i) {
        c = wlan_find_full_channel(ic,scan_order[i]);

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;
    }

    /* Iterate again adding half-rate and quarter-rate channels */
    for (i = 0; i < scan_order_size; ++i) {

        if (scan_order[i] < IEEE80211_2GHZ_FREQUENCY_THRESHOLD)
            continue;

        c = NULL;
        c = ieee80211_find_channel(ic, scan_order[i], 0,
                (IEEE80211_CHAN_A | IEEE80211_CHAN_HALF));
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;

        c = NULL;
        c = ieee80211_find_channel(ic, scan_order[i], 0,
                (IEEE80211_CHAN_A | IEEE80211_CHAN_QUARTER));

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;
    }

    /*
     * Configure 20, 40, 80, 160 MHz HT/VHT/HE channels if wide band
     * scan is supported by underlying phy and also enabled.
     */
    if (wlan_wide_band_scan_enabled(ic)) {
        /* Iterate again adding 20, 40 & 80 MHz channels */
        for (i = 0; i < scan_order_size; ++i) {
            /* Add 20 MHz channel */
            c = NULL;
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE20)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXA_HE20);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXG_HE20)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXG_HE20);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT20)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AC_VHT20);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT20)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11NA_HT20);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT20)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11NG_HT20);
            }
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;

            /* Add 5GHz 40+ channel */
            c = NULL;
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE40PLUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXA_HE40PLUS);
           } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT40PLUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AC_VHT40PLUS);
           } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT40PLUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11NA_HT40PLUS);
           }
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;

            /* Add 5 GHz 40- channels */
            c = NULL;
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE40MINUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXA_HE40MINUS);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT40MINUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AC_VHT40MINUS);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT40MINUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11NA_HT40MINUS);
            }
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;

            c = NULL;
            /* Add 2GHz 40+ channel */
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXG_HE40PLUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXG_HE40PLUS);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40PLUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11NG_HT40PLUS);
            }
            if (c)
                all_chans[nallchans++] = c;

            /* Add 2GHz 40- channel */
            c = NULL;
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXG_HE40MINUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXG_HE40MINUS);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40MINUS)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11NG_HT40MINUS);
            }
            if (c)
                all_chans[nallchans++] = c;

            /* Add 80 MHz channel */
            c = NULL;
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE80)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXA_HE80);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT80)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AC_VHT80);
            }
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;


#if ADD_160MHZ_CHANNELS
            /* Add 160 MHz channel */
            c = NULL;
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE160)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXA_HE160);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT160)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AC_VHT160);
            }
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
                all_chans[nallchans++] = c;
#endif

#if ADD_80P80_MHZ_CHANNELS
            /* Add 80+80 MHz channel */
            c = NULL;
            if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AXA_HE80_80)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AXA_HE80_80);
            } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11AC_VHT80_80)) {
                c = ieee80211_find_channel(ic, scan_order[i], 0, IEEE80211_CHAN_11AC_VHT80_80);
            }

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            if (c && !IEEE80211_IS_CHAN_RADAR(c))
#else
            if (c)
#endif
            {
                all_chans[nallchans++] = c;
            }
#endif
        }
    }

    param->nallchans = nallchans;

    for(i=0; i < nallchans; ++i) {
        c = all_chans[i];
        param->ch_param[i].mhz = c->ic_freq;
        param->ch_param[i].cfreq1 = c->ic_freq;
        param->ch_param[i].cfreq2 = 0;
        /* fill in mode. use 11G for 2G and 11A for 5G */

        if (c->ic_freq < 3000) {
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11G;
        } else {
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11A;
        }

        if (IEEE80211_IS_CHAN_PASSIVE(c)) {
            param->ch_param[i].is_chan_passive = TRUE;
        }

        if (ieee80211_find_channel(ic, c->ic_freq, 0, IEEE80211_CHAN_11AC_VHT20)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
        } else {
            temp_chan = ieee80211_find_channel(ic, c->ic_freq, 0, IEEE80211_CHAN_HT20);
            if (temp_chan) {
                param->ch_param[i].allow_ht = TRUE;
                if(ic->ic_vhtcap && IEEE80211_IS_CHAN_2GHZ(temp_chan))
                    param->ch_param[i].allow_vht = TRUE;
            }
        }
        if (IEEE80211_IS_CHAN_11AXA_HE80_80(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AX_HE80_80;
            param->ch_param[i].cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
            param->ch_param[i].cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
        } else if (IEEE80211_IS_CHAN_11AC_VHT80_80(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AC_VHT80_80;
            param->ch_param[i].cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
            param->ch_param[i].cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
        } else if (IEEE80211_IS_CHAN_11AXA_HE160(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AX_HE160;
            param->ch_param[i].cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
            param->ch_param[i].cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
        } else if (IEEE80211_IS_CHAN_11AC_VHT160(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AC_VHT160;
            param->ch_param[i].cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
            param->ch_param[i].cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
        } else if (IEEE80211_IS_CHAN_11AXA_HE80(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AX_HE80;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11AC_VHT80(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AC_VHT80;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11AXA_HE40(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AX_HE40;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11AXG_HE40(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AX_HE40_2G;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11AC_VHT40(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AC_VHT40;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11NA_HT40PLUS(c) ||
                IEEE80211_IS_CHAN_11NA_HT40MINUS(c)) {
            param->ch_param[i].allow_vht = FALSE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11NA_HT40;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11NG_HT40PLUS(c) ||
                IEEE80211_IS_CHAN_11NG_HT40MINUS(c)) {
            param->ch_param[i].allow_vht = FALSE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11NG_HT40;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11AXA_HE20(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AX_HE20;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11AXG_HE20(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AX_HE20_2G;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        } else if (IEEE80211_IS_CHAN_11AC_VHT20(c)) {
            param->ch_param[i].allow_vht = TRUE;
            param->ch_param[i].allow_ht = TRUE;
            param->ch_param[i].phy_mode = WMI_HOST_MODE_11AC_VHT20;
            param->ch_param[i].cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
            param->ch_param[i].cfreq2 = 0;
        }

        if (IEEE80211_IS_CHAN_HALF(c))
            param->ch_param[i].half_rate = TRUE;
        if (IEEE80211_IS_CHAN_QUARTER(c))
            param->ch_param[i].quarter_rate = TRUE;
        if (IEEE80211_IS_CHAN_DFS(c))
            param->ch_param[i].dfs_set = TRUE;
        if (IEEE80211_IS_CHAN_DFS_CFREQ2(c))
            param->ch_param[i].dfs_set_cfreq2 = TRUE;

        /* also fill in power information */
        param->ch_param[i].minpower = c->ic_minpower;
        param->ch_param[i].maxpower = c->ic_maxpower;
        param->ch_param[i].maxregpower = c->ic_maxregpower;
        param->ch_param[i].antennamax = c->ic_antennamax;
        param->ch_param[i].reg_class_id = c->ic_regClassId;
    }

	/* set correct pdev_id*/
	param->pdev_id = wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj);

    scan_info("num_chan: %d", nallchans);

    scan_tx_ops = wlan_get_scan_tx_ops(psoc);
    if (!scan_tx_ops->set_chan_list)
        goto fail;

    frag_list_bytes = (sizeof(struct scan_chan_list_params) +
            ((WMI_HOST_MAX_CHANS_PER_WMI_CMD - 1) *
             sizeof(struct channel_param)));

    frag_list = (struct scan_chan_list_params *)
        qdf_mem_malloc(frag_list_bytes);

    if (!frag_list)
        goto fail;

    send_idx = 0;
    do {
        qdf_mem_zero(frag_list, frag_list_bytes);
        frag_list->pdev_id = wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj);
        frag_list->nallchans =
            qdf_min(WMI_HOST_MAX_CHANS_PER_WMI_CMD,(nallchans - send_idx));
        if (send_idx)
            frag_list->append = true;
        else
            frag_list->append = false;

        qdf_mem_copy(&frag_list->ch_param[0], &param->ch_param[send_idx],
                sizeof(struct channel_param) * frag_list->nallchans);

        send_idx += frag_list->nallchans;

        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                "%s: nchans: %d: append:%d", __func__,
                frag_list->nallchans, frag_list->append);

        for (i = 0; i < frag_list->nallchans; i++)
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL,
                    IEEE80211_MSG_EXTIOCTL_CHANSSCAN,
                    "%d: cfreq1:%d, cfreq2:%d target_phymode:%d half: %d,"
                    " quarter: %d, ht: %d, vht: %d",
                    i, frag_list->ch_param[i].cfreq1,
                    frag_list->ch_param[i].cfreq2,
                    frag_list->ch_param[i].phy_mode,
                    frag_list->ch_param[i].half_rate,
                    frag_list->ch_param[i].quarter_rate,
                    frag_list->ch_param[i].allow_ht,
                    frag_list->ch_param[i].allow_vht);

        /* send chan list fragment to target */
        scan_tx_ops->set_chan_list(ic->ic_pdev_obj, frag_list);
    } while (send_idx < nallchans);

    qdf_mem_free(frag_list);

fail:
    qdf_mem_free(param);
    qdf_mem_free(all_chans);

    return QDF_STATUS_SUCCESS;
}

void wlan_scan_set_maxentry(struct ieee80211com *ic, uint16_t val)
{
    if (val < 10) {
        IEEE80211_SCANENTRY_PRINTF(ic, IEEE80211_MSG_SCANENTRY,
                "%s: Max Scan entry - Set value too low (%u). Limiting to 10.\n", __func__, val);
        val = 10;
    }

    /* set max scan entry to scan obj */
    ic->ic_scan_entry_max_count = val;
}

void wlan_scan_set_timeout(struct ieee80211com *ic, uint16_t val)
{
    /* Max timeout value is 10 mins */
    if (val > 0) {
        if (val > 600) {
            IEEE80211_SCANENTRY_PRINTF(ic, IEEE80211_MSG_SCANENTRY,
                    "%s: Scan entry timeout - Set value too high (%u). Limiting to 600.\n", __func__, val);
            val = 600;
        }
        /* set max scan entry to scan obj */
        ic->ic_scan_entry_timeout = val;
    }
}

uint16_t
wlan_scan_get_timeout(struct ieee80211com *ic)
{
    /* return max scan entry to scan obj */
    return ic->ic_scan_entry_timeout;
}

uint16_t
wlan_scan_get_maxentry(struct ieee80211com *ic)
{
    /* get max scan entry to scan obj */
    return ic->ic_scan_entry_max_count;
}

bool wlan_scan_in_progress(wlan_if_t vap)
{
    return (ucfg_scan_get_pdev_status(wlan_vdev_get_pdev(vap->vdev_obj))
            != SCAN_NOT_IN_PROGRESS);
}

bool wlan_pdev_scan_in_progress(struct wlan_objmgr_pdev *pdev)
{
    return (ucfg_scan_get_pdev_status(pdev)
            != SCAN_NOT_IN_PROGRESS);
}

bool wlan_vdev_scan_in_progress(struct wlan_objmgr_vdev *vdev)
{
    return (ucfg_scan_get_vdev_status(vdev)
            != SCAN_NOT_IN_PROGRESS);
}

uint32_t ieee80211_channel_frequency(struct ieee80211_ath_channel *chan)
{
    return chan->ic_freq;    /* frequency in Mhz */
}

uint32_t ieee80211_channel_ieee(struct ieee80211_ath_channel *chan)
{
    return chan->ic_ieee;    /* channel number */
}

QDF_STATUS
wlan_util_scan_entry_update_mlme_info(wlan_if_t vap,
        struct scan_cache_entry *scan_entry)
{
    struct wlan_objmgr_pdev *pdev;

    pdev = wlan_vap_get_pdev(vap);
    return util_scan_entry_update_mlme_info(pdev, scan_entry);
}

bool wlan_wide_band_scan_enabled(struct ieee80211com *ic)
{
    if (ic->wide_band_scan_enabled)
        return ic->wide_band_scan_enabled(ic);
    else
        return false;
}

QDF_STATUS
wlan_scan_update_wide_band_scan_config(struct ieee80211com *ic)
{
    return ucfg_scan_set_wide_band_scan(ic->ic_pdev_obj,
            wlan_wide_band_scan_enabled(ic));
}

#if WLAN_DFS_CHAN_HIDDEN_SSID
QDF_STATUS
wlan_scan_config_ssid_bssid_hidden_ssid_beacon(struct ieee80211com *ic,
        ieee80211_ssid *conf_ssid, uint8_t *bssid)
{
    struct wlan_ssid *ssid;

    ssid = convert_ieee_ssid_to_wlan_ssid_inplace(1, conf_ssid);
    if(ssid == NULL) {
        qdf_print("%s ssid NULL ",__func__);
        return QDF_STATUS_E_INVAL;
    }
    qdf_print("%s: conf ssid:%.*s conf bssid:%s ",
            __func__, ssid->length, ssid->ssid, ether_sprintf(bssid));

    return ucfg_scan_config_hidden_ssid_for_bssid(ic->ic_pdev_obj,
            bssid, ssid);
}
#endif /* WLAN_DFS_CHAN_HIDDEN_SSID */
