/*
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
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

/*
 *  all the IE parsing/processing routines.
 */
#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include "ieee80211_mlme_priv.h"
#include "ieee80211_proto.h"
#include "ol_if_athvar.h"
#include "ieee80211_mlme_dfs_dispatcher.h"
#include <wlan_lmac_if_api.h>
#include <wlan_objmgr_psoc_obj.h>

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif

#include <wlan_son_pub.h>

#include <target_if.h>

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif
#include <wlan_dfs_ioctl.h>
#include <wlan_mlme_dp_dispatcher.h>
#ifdef WLAN_SUPPORT_FILS
#include <wlan_fd_utils_api.h>
#endif
#include <wlan_utility.h>
#include "cfg_ucfg_api.h"
#include <ieee80211.h>
#include <wlan_vdev_mlme.h>

#define	IEEE80211_ADDSHORT(frm, v) 	do {frm[0] = (v) & 0xff; frm[1] = (v) >> 8;	frm += 2;} while (0)
#define	IEEE80211_ADDSELECTOR(frm, sel) do {OS_MEMCPY(frm, sel, 4); frm += 4;} while (0)
#define IEEE80211_SM(_v, _f)    (((_v) << _f##_S) & _f)
#define IEEE80211_MS(_v, _f)    (((_v) & _f) >> _f##_S)
#define RX_MCS_SINGLE_STREAM_BYTE_OFFSET            (0)
#define RX_MCS_DUAL_STREAM_BYTE_OFFSET              (1)
#define RX_MCS_ALL_NSTREAM_RATES                    (0xff)

#define MAX_ABS_NEG_PWR   64  /* Maximum Absolute Negative power in dB*/
#define MAX_ABS_POS_PWR   63  /* Maximum Absolute Positive power in dB*/
#define TWICE_MAX_POS_PWR 127 /* Twice Maximum positive power in dB */

void ieee80211_savenie(osdev_t osdev,u_int8_t **iep,
        const u_int8_t *ie, u_int ielen);
void ieee80211_saveie(osdev_t osdev,u_int8_t **iep, const u_int8_t *ie);
u_int8_t * ieee80211_add_vht_wide_bw_switch(u_int8_t *frm,
        struct ieee80211_node *ni, struct ieee80211com *ic,
        u_int8_t subtype, int is_vendor_ie);

/*
 * Add a supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_rates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
    int nrates;

    *frm++ = IEEE80211_ELEMID_RATES;
    nrates = rs->rs_nrates;
    if (nrates > IEEE80211_RATE_SIZE)
        nrates = IEEE80211_RATE_SIZE;
    *frm++ = nrates;
    OS_MEMCPY(frm, rs->rs_rates, nrates);
    return frm + nrates;
}

/*
 * Add an extended supported rates element id to a frame.
 */
u_int8_t *
ieee80211_add_xrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
    /*
     * Add an extended supported rates element if operating in 11g mode.
     */
    if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
        int nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
        *frm++ = IEEE80211_ELEMID_XRATES;
        *frm++ = nrates;
        OS_MEMCPY(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
        frm += nrates;
    }
    return frm;
}

/*
 * Add an ssid elemet to a frame.
 */
u_int8_t *
ieee80211_add_ssid(u_int8_t *frm, const u_int8_t *ssid, u_int len)
{
    *frm++ = IEEE80211_ELEMID_SSID;
    *frm++ = len;
    OS_MEMCPY(frm, ssid, len);
    return frm + len;
}

/*
 * Add an erp element to a frame.
 */
u_int8_t *
ieee80211_add_erp(u_int8_t *frm, struct ieee80211com *ic)
{
    u_int8_t erp;

    *frm++ = IEEE80211_ELEMID_ERP;
    *frm++ = 1;
    erp = 0;
    if (ic->ic_nonerpsta != 0 )
        erp |= IEEE80211_ERP_NON_ERP_PRESENT;
    if (ic->ic_flags & IEEE80211_F_USEPROT)
        erp |= IEEE80211_ERP_USE_PROTECTION;
    if (ic->ic_flags & IEEE80211_F_USEBARKER)
        erp |= IEEE80211_ERP_LONG_PREAMBLE;
    *frm++ = erp;
    return frm;
}

/*
 * Add a country information element to a frame.
 */
u_int8_t *
ieee80211_add_country(u_int8_t *frm, struct ieee80211vap *vap)
{
    u_int16_t chanflags;
    struct ieee80211com *ic = vap->iv_ic;
    uint8_t ctry_iso[REG_ALPHA2_LEN + 1];

    /* add country code */
    if(IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan))
        chanflags = IEEE80211_CHAN_2GHZ;
    else
        chanflags = IEEE80211_CHAN_5GHZ;

    ieee80211_getCurrentCountryISO(ic, ctry_iso);

    if (chanflags != vap->iv_country_ie_chanflags)
        ieee80211_build_countryie(vap, ctry_iso);

    if (vap->iv_country_ie_data.country_len) {
    	OS_MEMCPY(frm, (u_int8_t *)&vap->iv_country_ie_data,
	    	vap->iv_country_ie_data.country_len + 2);
	    frm +=  vap->iv_country_ie_data.country_len + 2;
    }

    return frm;
}

#if ATH_SUPPORT_IBSS_DFS
/*
* Add a IBSS DFS element into a frame
*/
u_int8_t *
ieee80211_add_ibss_dfs(u_int8_t *frm, struct ieee80211vap *vap)
{

    OS_MEMCPY(frm, (u_int8_t *)&vap->iv_ibssdfs_ie_data,
              vap->iv_ibssdfs_ie_data.len + sizeof(struct ieee80211_ie_header));
    frm += vap->iv_ibssdfs_ie_data.len + sizeof(struct ieee80211_ie_header);

    return frm;
}

u_int8_t *
ieee80211_add_ibss_csa(u_int8_t *frm, struct ieee80211vap *vap)
{

    OS_MEMCPY(frm, (u_int8_t *)&vap->iv_channelswitch_ie_data,
              vap->iv_channelswitch_ie_data.len + sizeof(struct ieee80211_ie_header));
    frm += vap->iv_channelswitch_ie_data.len + sizeof(struct ieee80211_ie_header);

    return frm;
}
#endif /* ATH_SUPPORT_IBSS_DFS */

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
u_int8_t *
ieee80211_setup_wpa_ie(struct ieee80211vap *vap, u_int8_t *ie)
{
    static const u_int8_t oui[4] = { WPA_OUI_BYTES, WPA_OUI_TYPE };
    static const u_int8_t cipher_suite[IEEE80211_CIPHER_MAX][4] = {
        { WPA_OUI_BYTES, WPA_CSE_WEP40 },    /* NB: 40-bit */
        { WPA_OUI_BYTES, WPA_CSE_TKIP },
        { 0x00, 0x00, 0x00, 0x00 },        /* XXX WRAP */
        { WPA_OUI_BYTES, WPA_CSE_CCMP },
        { 0x00, 0x00, 0x00, 0x00 },        /* XXX CKIP */
        { WPA_OUI_BYTES, WPA_CSE_NULL },
    };
    static const u_int8_t wep104_suite[4] =
        { WPA_OUI_BYTES, WPA_CSE_WEP104 };
    static const u_int8_t key_mgt_unspec[4] =
        { WPA_OUI_BYTES, AKM_SUITE_TYPE_IEEE8021X };
    static const u_int8_t key_mgt_psk[4] =
        { WPA_OUI_BYTES, AKM_SUITE_TYPE_PSK };
    static const u_int8_t key_mgt_cckm[4] =
        { CCKM_OUI_BYTES, CCKM_ASE_UNSPEC };
    const struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_cipher_type mcastcipher;
    u_int8_t *frm = ie;
    u_int8_t *selcnt;
    *frm++ = IEEE80211_ELEMID_VENDOR;
    *frm++ = 0;                /* length filled in below */
    OS_MEMCPY(frm, oui, sizeof(oui));        /* WPA OUI */
    frm += sizeof(oui);
    IEEE80211_ADDSHORT(frm, WPA_VERSION);

    /* XXX filter out CKIP */

    /* multicast cipher */
    mcastcipher = ieee80211_get_current_mcastcipher(vap);
    if (mcastcipher == IEEE80211_CIPHER_WEP &&
        rsn->rsn_mcastkeylen >= 13)
        IEEE80211_ADDSELECTOR(frm, wep104_suite);
    else{
            if(mcastcipher < IEEE80211_CIPHER_NONE)
                    IEEE80211_ADDSELECTOR(frm, cipher_suite[mcastcipher]);
    }

    /* unicast cipher list */
    selcnt = frm;
    IEEE80211_ADDSHORT(frm, 0);			/* selector count */
/* do not use CCMP unicast cipher in WPA mode */
#if IEEE80211_USE_WPA_CCMP
    if (RSN_CIPHER_IS_CCMP128(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
    }
    if (RSN_CIPHER_IS_CCMP256(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM_256]);
    }
    if (RSN_CIPHER_IS_GCMP128(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_GCM]);
    }
    if (RSN_CIPHER_IS_GCMP256(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_GCM_256]);
    }
#endif
    if (RSN_CIPHER_IS_TKIP(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
    }

    /* authenticator selector list */
    selcnt = frm;
	IEEE80211_ADDSHORT(frm, 0);			/* selector count */
    if (RSN_AUTH_IS_CCKM(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, key_mgt_cckm);
    } else {
    if (rsn->rsn_keymgmtset & WPA_ASE_8021X_UNSPEC) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, key_mgt_unspec);
    }
    if (rsn->rsn_keymgmtset & WPA_ASE_8021X_PSK) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, key_mgt_psk);
    }
    }

    /* optional capabilities */
    if (rsn->rsn_caps != 0 && rsn->rsn_caps != RSN_CAP_PREAUTH)
        IEEE80211_ADDSHORT(frm, rsn->rsn_caps);

    /* calculate element length */
    ie[1] = frm - ie - 2;
    KASSERT(ie[1]+2 <= sizeof(struct ieee80211_ie_wpa),
            ("WPA IE too big, %u > %zu", ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
    return frm;
}
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
u_int8_t *
ieee80211_setup_rsn_ie(struct ieee80211vap *vap, u_int8_t *ie)
{
    static const u_int8_t cipher_suite[][4] = {
        { RSN_OUI_BYTES, RSN_CSE_WEP40 },    /* NB: 40-bit */
        { RSN_OUI_BYTES, RSN_CSE_TKIP },
        { RSN_OUI_BYTES, RSN_CSE_WRAP },
        { RSN_OUI_BYTES, RSN_CSE_CCMP },
        { RSN_OUI_BYTES, RSN_CSE_CCMP },   /* WAPI */
        { CCKM_OUI_BYTES, RSN_CSE_NULL },  /* XXX CKIP */
        { RSN_OUI_BYTES, RSN_CSE_AES_CMAC }, /* AES_CMAC */
        { RSN_OUI_BYTES, RSN_CSE_CCMP_256 },   /* CCMP 256 */
        { RSN_OUI_BYTES, RSN_CSE_BIP_CMAC_256 }, /* AES_CMAC 256*/
        { RSN_OUI_BYTES, RSN_CSE_GCMP_128 },
        { RSN_OUI_BYTES, RSN_CSE_GCMP_256 },   /* GCMP 256 */
        { RSN_OUI_BYTES, RSN_CSE_BIP_GMAC_128 }, /* AES_GMAC */
        { RSN_OUI_BYTES, RSN_CSE_BIP_GMAC_256 }, /* AES_GMAC 256*/
        { RSN_OUI_BYTES, RSN_CSE_NULL },
    };
    static const u_int8_t wep104_suite[4] =
        { RSN_OUI_BYTES, RSN_CSE_WEP104 };
    static const u_int8_t key_mgt_unspec[4] =
        { RSN_OUI_BYTES, AKM_SUITE_TYPE_IEEE8021X };
    static const u_int8_t key_mgt_psk[4] =
        { RSN_OUI_BYTES, AKM_SUITE_TYPE_PSK };
    static const u_int8_t key_mgt_sha256_1x[4] =
        { RSN_OUI_BYTES, AKM_SUITE_TYPE_SHA256_IEEE8021X };
    static const u_int8_t key_mgt_sha256_psk[4] =
        { RSN_OUI_BYTES, AKM_SUITE_TYPE_SHA256_PSK };
    static const u_int8_t key_mgt_cckm[4] =
        { CCKM_OUI_BYTES, CCKM_ASE_UNSPEC };
    const struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_cipher_type mcastcipher;
    u_int8_t *frm = ie;
    u_int8_t *selcnt, pmkidFilled=0;
    int i;

    *frm++ = IEEE80211_ELEMID_RSN;
    *frm++ = 0;                /* length filled in below */
    IEEE80211_ADDSHORT(frm, RSN_VERSION);

    /* XXX filter out CKIP */

    /* multicast cipher */
    mcastcipher = ieee80211_get_current_mcastcipher(vap);
    if (mcastcipher == IEEE80211_CIPHER_WEP &&
        rsn->rsn_mcastkeylen >= 13) {
        IEEE80211_ADDSELECTOR(frm, wep104_suite);
    } else {
        IEEE80211_ADDSELECTOR(frm, cipher_suite[mcastcipher]);
    }

    /* unicast cipher list */
    selcnt = frm;
    IEEE80211_ADDSHORT(frm, 0);			/* selector count */
    if (RSN_CIPHER_IS_CCMP128(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM]);
    }
   if (RSN_CIPHER_IS_CCMP256(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CCM_256]);
    }
    if (RSN_CIPHER_IS_GCMP128(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_GCM]);
    }
    if (RSN_CIPHER_IS_GCMP256(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_GCM_256]);
    }

    if (RSN_CIPHER_IS_TKIP(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_TKIP]);
    }

    /* authenticator selector list */
    selcnt = frm;
	IEEE80211_ADDSHORT(frm, 0);			/* selector count */
    if (RSN_AUTH_IS_CCKM(rsn)) {
        selcnt[0]++;
        IEEE80211_ADDSELECTOR(frm, key_mgt_cckm);
    } else {
        if (rsn->rsn_keymgmtset & RSN_ASE_8021X_UNSPEC) {
            selcnt[0]++;
            IEEE80211_ADDSELECTOR(frm, key_mgt_unspec);
        }
        if (rsn->rsn_keymgmtset & RSN_ASE_8021X_PSK) {
            selcnt[0]++;
            IEEE80211_ADDSELECTOR(frm, key_mgt_psk);
        }
        if (rsn->rsn_keymgmtset & RSN_ASE_SHA256_IEEE8021X) {
            selcnt[0]++;
            IEEE80211_ADDSELECTOR(frm, key_mgt_sha256_1x);
        }
        if (rsn->rsn_keymgmtset & RSN_ASE_SHA256_PSK) {
            selcnt[0]++;
            IEEE80211_ADDSELECTOR(frm, key_mgt_sha256_psk);
        }
    }

    /* capabilities */
    IEEE80211_ADDSHORT(frm, rsn->rsn_caps);

    /* PMKID */
    if (vap->iv_opmode == IEEE80211_M_STA && vap->iv_pmkid_count > 0) {
        struct ieee80211_node *ni = vap->iv_bss; /* bss node */
        /* Find and include the PMKID for target AP*/
        for (i = 0; i < vap->iv_pmkid_count; i++) {
            if (!OS_MEMCMP( vap->iv_pmkid_list[i].bssid, ni->ni_bssid, IEEE80211_ADDR_LEN)) {
                IEEE80211_ADDSHORT(frm, 1);
                OS_MEMCPY(frm, vap->iv_pmkid_list[i].pmkid, IEEE80211_PMKID_LEN);
                frm += IEEE80211_PMKID_LEN;
                pmkidFilled = 1;
                break;
            }
        }
    }

    /* mcast/group mgmt cipher set (optional 802.11w) */
    if ((vap->iv_opmode == IEEE80211_M_HOSTAP)  &&  (rsn->rsn_caps & RSN_CAP_MFP_ENABLED)) {
        if (!pmkidFilled) {
            /* PMKID is not filled. so put zero for PMKID count */
            IEEE80211_ADDSHORT(frm, 0);
        }
        if(RSN_CIPHER_IS_CMAC(rsn))
            IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CMAC]);
        else if(RSN_CIPHER_IS_CMAC256(rsn))
            IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_CMAC_256]);
        else if(RSN_CIPHER_IS_GMAC(rsn))
            IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_GMAC]);
        else if(RSN_CIPHER_IS_GMAC256(rsn))
            IEEE80211_ADDSELECTOR(frm, cipher_suite[IEEE80211_CIPHER_AES_GMAC_256]);
    }

    /* calculate element length */
    ie[1] = frm - ie - 2;
    KASSERT(ie[1]+2 <= sizeof(struct ieee80211_ie_wpa),
            ("RSN IE too big, %u > %zu", ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
    return frm;
}
#endif
u_int8_t
ieee80211_get_rxstreams(struct ieee80211com *ic, struct ieee80211vap *vap)
{
    u_int8_t rx_streams = 0;

    rx_streams = ieee80211_getstreams(ic, ic->ic_rx_chainmask);
#if ATH_SUPPORT_WAPI
    if(IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
       && (RSN_CIPHER_IS_SMS4(&vap->iv_rsn))) {
#else
      ) {
#endif
        if (rx_streams > ic->ic_num_wapi_rx_maxchains)
            rx_streams = ic->ic_num_wapi_rx_maxchains;
    }
#endif
    return rx_streams;
}

u_int8_t
ieee80211_get_txstreams(struct ieee80211com *ic, struct ieee80211vap *vap)
{
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;
    u_int8_t tx_streams = 0;

    if(ic->ic_is_mode_offload(ic) && vdev_mlme->proto.generic.nss != 0){
        tx_streams = MIN(vdev_mlme->proto.generic.nss,
                ieee80211_getstreams(ic, ic->ic_tx_chainmask));
    }else{
        tx_streams = ieee80211_getstreams(ic, ic->ic_tx_chainmask);
    }
#if ATH_SUPPORT_WAPI
    if(IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
       && (RSN_CIPHER_IS_SMS4(&vap->iv_rsn))) {
#else
      ) {
#endif
        if (tx_streams > ic->ic_num_wapi_tx_maxchains)
            tx_streams = ic->ic_num_wapi_tx_maxchains;
    }
#endif
    return tx_streams;
}

/* add IE for WAPI in mgmt frames */
#if ATH_SUPPORT_WAPI
u_int8_t *
ieee80211_setup_wapi_ie(struct ieee80211vap *vap, u_int8_t *ie)
{
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    return wlan_crypto_build_wapiie(vap->vdev_obj, ie);
#else
#define	ADDSHORT(frm, v) do {frm[0] = (v) & 0xff;frm[1] = (v) >> 8;frm += 2;} while (0)
#define	ADDSELECTOR(frm, sel) do {OS_MEMCPY(frm, sel, 4); frm += 4;} while (0)
#define	WAPI_OUI_BYTES		0x00, 0x14, 0x72

	static const u_int8_t cipher_suite[4] =
		{ WAPI_OUI_BYTES, WAPI_CSE_WPI_SMS4};	/* SMS4 128 bits */
	static const u_int8_t key_mgt_unspec[4] =
		{ WAPI_OUI_BYTES, WAPI_ASE_WAI_UNSPEC };
	static const u_int8_t key_mgt_psk[4] =
		{ WAPI_OUI_BYTES, WAPI_ASE_WAI_PSK };
	const struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
	u_int8_t *frm = ie;
	u_int8_t *selcnt;
	*frm++ = IEEE80211_ELEMID_WAPI;
	*frm++ = 0;				/* length filled in below */
	ADDSHORT(frm, WAPI_VERSION);

	/* authenticator selector list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */

	if (rsn->rsn_keymgmtset & WAPI_ASE_WAI_UNSPEC) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_unspec);
	}
	if (rsn->rsn_keymgmtset & WAPI_ASE_WAI_PSK) {
		selcnt[0]++;
		ADDSELECTOR(frm, key_mgt_psk);
	}

	/* unicast cipher list */
	selcnt = frm;
	ADDSHORT(frm, 0);			/* selector count */

	if (RSN_HAS_UCAST_CIPHER(rsn, IEEE80211_CIPHER_WAPI)) {
		selcnt[0]++;
		ADDSELECTOR(frm, cipher_suite);
	}

	/* multicast cipher */
	ADDSELECTOR(frm, cipher_suite);

	/* optional capabilities */
	ADDSHORT(frm, rsn->rsn_caps);
	/* XXX PMKID */

    /* BKID count, only in ASSOC/REASSOC REQ frames from STA to AP*/
    if (vap->iv_opmode == IEEE80211_M_STA) {
        ADDSHORT(frm, 0);
    }

	/* calculate element length */
	ie[1] = frm - ie - 2;
	KASSERT(ie[1]+2 <= sizeof(struct ieee80211_ie_wpa),
		("RSN IE too big, %u > %zu",
		ie[1]+2, sizeof(struct ieee80211_ie_wpa)));
	return frm;
#undef ADDSELECTOR
#undef ADDSHORT
#undef WAPI_OUI_BYTES
#endif
}
#endif /*ATH_SUPPORT_WAPI*/

/*
 * Add a WME Info element to a frame.
 */
u_int8_t *
ieee80211_add_wmeinfo(u_int8_t *frm, struct ieee80211_node *ni,
                      u_int8_t wme_subtype, u_int8_t *wme_info, u_int8_t info_len)
{
    static const u_int8_t oui[4] = { WME_OUI_BYTES, WME_OUI_TYPE };
    struct ieee80211_ie_wme *ie = (struct ieee80211_ie_wme *) frm;
    struct ieee80211_wme_state *wme = &ni->ni_ic->ic_wme;
    struct ieee80211vap *vap = ni->ni_vap;

    *frm++ = IEEE80211_ELEMID_VENDOR;
    *frm++ = 0;                             /* length filled in below */
    OS_MEMCPY(frm, oui, sizeof(oui));       /* WME OUI */
    frm += sizeof(oui);
    *frm++ = wme_subtype;          /* OUI subtype */
    switch (wme_subtype) {
    case WME_INFO_OUI_SUBTYPE:
        *frm++ = WME_VERSION;                   /* protocol version */
        /* QoS Info field depends on operating mode */
        ie->wme_info = 0;
        switch (vap->iv_opmode) {
        case IEEE80211_M_HOSTAP:
            *frm = wme->wme_bssChanParams.cap_info & WME_QOSINFO_COUNT;
            if (IEEE80211_VAP_IS_UAPSD_ENABLED(vap)) {
                *frm |= WME_CAPINFO_UAPSD_EN;
            }
            frm++;
            break;
        case IEEE80211_M_STA:
            /* Set the U-APSD flags */
            if (ieee80211_vap_wme_is_set(vap) && (ni->ni_ext_caps & IEEE80211_NODE_C_UAPSD)) {
                *frm |= vap->iv_uapsd;
            }
            frm++;
            break;
        default:
            *frm++ = 0;
        }
        break;
    case WME_TSPEC_OUI_SUBTYPE:
        *frm++ = WME_TSPEC_OUI_VERSION;        /* protocol version */
        OS_MEMCPY(frm, wme_info, info_len);
        frm += info_len;
        break;
    default:
        break;
    }

    ie->wme_len = (u_int8_t)(frm - &ie->wme_oui[0]);

    return frm;
}

/*
 * Add a WME Parameter element to a frame.
 */
u_int8_t *
ieee80211_add_wme_param(u_int8_t *frm, struct ieee80211_wme_state *wme,
                        int uapsd_enable)
{
    static const u_int8_t oui[4] = { WME_OUI_BYTES, WME_OUI_TYPE };
    struct ieee80211_wme_param *ie = (struct ieee80211_wme_param *) frm;
    int i;

    *frm++ = IEEE80211_ELEMID_VENDOR;
    *frm++ = 0;				/* length filled in below */
    OS_MEMCPY(frm, oui, sizeof(oui));		/* WME OUI */
    frm += sizeof(oui);
    *frm++ = WME_PARAM_OUI_SUBTYPE;		/* OUI subtype */
    *frm++ = WME_VERSION;			/* protocol version */

    ie->param_qosInfo = 0;
    *frm = wme->wme_bssChanParams.cap_info & WME_QOSINFO_COUNT;
    if (uapsd_enable) {
        *frm |= WME_CAPINFO_UAPSD_EN;
    }
    frm++;
    *frm++ = 0;                             /* reserved field */
    for (i = 0; i < WME_NUM_AC; i++) {
        const struct wmeParams *ac =
            &wme->wme_bssChanParams.cap_wmeParams[i];
        *frm++ = IEEE80211_SM(i, WME_PARAM_ACI)
            | IEEE80211_SM(ac->wmep_acm, WME_PARAM_ACM)
            | IEEE80211_SM(ac->wmep_aifsn, WME_PARAM_AIFSN)
            ;
        *frm++ = IEEE80211_SM(ac->wmep_logcwmax, WME_PARAM_LOGCWMAX)
            | IEEE80211_SM(ac->wmep_logcwmin, WME_PARAM_LOGCWMIN)
            ;
        IEEE80211_ADDSHORT(frm, ac->wmep_txopLimit);
    }

    ie->param_len = frm - &ie->param_oui[0];

    return frm;
}

u_int8_t *
ieee80211_add_muedca_param(u_int8_t *frm, struct ieee80211_muedca_state *muedca)
{
    struct ieee80211_ie_muedca *ie = (struct ieee80211_ie_muedca *)frm;
    u_int8_t muedca_ie_len = sizeof(struct ieee80211_ie_muedca);
    int iter;

    ie->muedca_id = IEEE80211_ELEMID_EXTN;
    ie->muedca_len = IEEE80211_MUEDCA_LENGTH;
    ie->muedca_id_ext = IEEE80211_ELEMID_EXT_MUEDCA;
    ie->muedca_qosinfo = IEEE80211_SM(muedca->muedca_param_update_count,
            IEEE80211_MUEDCA_UPDATE_COUNT);

    for(iter = 0; iter < MUEDCA_NUM_AC; iter++) {
        ie->muedca_param_record[iter].aifsn_aci =
            IEEE80211_SM(muedca->muedca_paramList[iter].muedca_aifsn,
                    IEEE80211_MUEDCA_AIFSN) |
            IEEE80211_SM(muedca->muedca_paramList[iter].muedca_acm,
                    IEEE80211_MUEDCA_ACM) |
            IEEE80211_SM(iter, IEEE80211_MUEDCA_ACI);

        ie->muedca_param_record[iter].ecwminmax =
            IEEE80211_SM(muedca->muedca_paramList[iter].muedca_ecwmin,
                    IEEE80211_MUEDCA_ECWMIN) |
            IEEE80211_SM(muedca->muedca_paramList[iter].muedca_ecwmax,
                    IEEE80211_MUEDCA_ECWMAX);

        ie->muedca_param_record[iter].timer =
            muedca->muedca_paramList[iter].muedca_timer;

    }

    return frm + muedca_ie_len;
}

/*
 * Add an Atheros Advanaced Capability element to a frame
 */
u_int8_t *
ieee80211_add_athAdvCap(u_int8_t *frm, u_int8_t capability, u_int16_t defaultKey)
{
    static const u_int8_t oui[6] = {(ATH_OUI & 0xff), ((ATH_OUI >>8) & 0xff),
                                    ((ATH_OUI >> 16) & 0xff), ATH_OUI_TYPE, ATH_OUI_SUBTYPE, ATH_OUI_VERSION};
    struct ieee80211_ie_athAdvCap *ie = (struct ieee80211_ie_athAdvCap *) frm;

    *frm++ = IEEE80211_ELEMID_VENDOR;
    *frm++ = 0;				/* Length filled in below */
    OS_MEMCPY(frm, oui, sizeof(oui));		/* Atheros OUI, type, subtype, and version for adv capabilities */
    frm += sizeof(oui);
    *frm++ = capability;

    /* Setup default key index in little endian byte order */
    *frm++ = (defaultKey & 0xff);
    *frm++ = ((defaultKey >> 8)& 0xff);
    ie->athAdvCap_len = frm - &ie->athAdvCap_oui[0];

    return frm;
}

/*
 *  Add a QCA bandwidth-NSS Mapping information element to a frame
 */
u_int8_t *
ieee80211_add_bw_nss_maping(u_int8_t *frm, struct ieee80211_bwnss_map *bw_nss_mapping)
{
    static const u_int8_t oui[6] = {(ATH_OUI & 0xff), ((ATH_OUI >>8) & 0xff),
                                   ((ATH_OUI >> 16) & 0xff),ATH_OUI_BW_NSS_MAP_TYPE,
                                       ATH_OUI_BW_NSS_MAP_SUBTYPE, ATH_OUI_BW_NSS_VERSION};
    struct ieee80211_bwnss_mapping *ie = (struct ieee80211_bwnss_mapping *) frm;

    *frm++ = IEEE80211_ELEMID_VENDOR;
    *frm++ = 0x00;                            /* Length filled in below */
    OS_MEMCPY(frm, oui, IEEE80211_N(oui));    /* QCA OUI, type, sub-type, version, bandwidth NSS mapping Vendor specific IE  */
    frm += IEEE80211_N(oui);
    *frm++ = IEEE80211_BW_NSS_ADV_160(bw_nss_mapping->bw_nss_160); /* XXX: Add higher BW map if and when required */

    ie->bnm_len = (frm - &ie->bnm_oui[0]);
    return frm;
}

u_int8_t* add_chan_switch_ie(u_int8_t *frm, struct ieee80211_ath_channel *next_ch, u_int8_t tbtt_cnt)
{
    struct ieee80211_ath_channelswitch_ie *csaie = (struct ieee80211_ath_channelswitch_ie*)frm;
    int csaielen = sizeof(struct ieee80211_ath_channelswitch_ie);

    csaie->ie = IEEE80211_ELEMID_CHANSWITCHANN;
    csaie->len = 3; /* fixed len */
    csaie->switchmode = 1; /* AP forces STAs in the BSS to stop transmissions until the channel switch completes*/
    csaie->newchannel = next_ch->ic_ieee;
    csaie->tbttcount = tbtt_cnt;

    return frm + csaielen;
}

u_int8_t* add_sec_chan_offset_ie(u_int8_t *frm, struct ieee80211_ath_channel *next_ch)
{
    struct ieee80211_ie_sec_chan_offset *secie = (struct ieee80211_ie_sec_chan_offset*)frm;
    int secielen = sizeof(struct ieee80211_ie_sec_chan_offset);

    secie->elem_id = IEEE80211_ELEMID_SECCHANOFFSET;
    secie->len = 1;
    secie->sec_chan_offset = ieee80211_sec_chan_offset(next_ch);

    return frm + secielen;
}

u_int8_t* add_build_version_ie(u_int8_t *frm, struct ieee80211com *ic)
{
    struct ieee80211_build_version_ie *bvie =
        (struct ieee80211_build_version_ie*)frm;
    int bvielen = sizeof(struct ieee80211_build_version_ie);

    OS_MEMZERO(bvie->build_variant, sizeof(bvie->build_variant));
    OS_MEMCPY(bvie->build_variant, SW_BUILD_VARIANT, SW_BUILD_VARIANT_LEN);
    bvie->sw_build_version = SW_BUILD_VERSION;
    bvie->sw_build_maj_ver = SW_BUILD_MAJ_VER;
    bvie->sw_build_min_ver = SW_BUILD_MIN_VER;
    bvie->sw_build_rel_variant = SW_BUILD_REL_VARIANT;
    bvie->sw_build_rel_num = SW_BUILD_REL_NUM;
    bvie->chip_vendorid = htole32(ic->vendor_id);
    bvie->chip_devid = htole32(ic->device_id);

    return frm + bvielen;
}

u_int8_t *
ieee80211_add_sw_version_ie(u_int8_t *frm, struct ieee80211com *ic)
{
    u_int8_t *t_frm;
    u_int8_t ie_len;
    static const u_int8_t oui[QCA_OUI_LEN] = {
        (QCA_OUI & QCA_OUI_BYTE_MASK),
        ((QCA_OUI >> QCA_OUI_ONE_BYTE_SHIFT) & QCA_OUI_BYTE_MASK),
        ((QCA_OUI >> QCA_OUI_TWO_BYTE_SHIFT) & QCA_OUI_BYTE_MASK),
        QCA_OUI_GENERIC_TYPE_1, QCA_OUI_BUILD_INFO_SUBTYPE,
        QCA_OUI_BUILD_INFO_VERSION
    };

    t_frm = frm;
     /* reserving 2 bytes for the element id and element len*/
    frm += IE_LEN_ID_LEN;

    OS_MEMCPY(frm, oui, IEEE80211_N(oui));
    frm += IEEE80211_N(oui);

    frm = add_build_version_ie(frm, ic);

    ie_len = frm - t_frm - IE_LEN_ID_LEN;
    *t_frm++ = IEEE80211_ELEMID_VENDOR;
    *t_frm = ie_len;
    /* updating efrm with actual index*/
    t_frm = frm;

    return t_frm;
}

/*
 *  Add next channel info in beacon vendor IE, this will be used when RE detects RADAR and Root does not
 *  RE send RCSAs to Root and CSAs to its clients and switches to the channel that was communicated by its
 *  parent through this vendor IE
 */
u_int8_t *
ieee80211_add_next_channel(u_int8_t *frm, struct ieee80211_node *ni, struct ieee80211com *ic, int subtype)
{
    u_int8_t *efrm;
    u_int8_t ie_len;
    u_int16_t chwidth;
    struct ieee80211_ie_wide_bw_switch *widebw = NULL;
    int widebw_len = sizeof(struct ieee80211_ie_wide_bw_switch);
    struct ieee80211_ath_channel *next_channel = ic->ic_tx_next_ch;
    static const u_int8_t oui[6] = {
        (QCA_OUI & 0xff), ((QCA_OUI >> 8) & 0xff), ((QCA_OUI >> 16) & 0xff),
        QCA_OUI_NC_TYPE, QCA_OUI_NC_SUBTYPE,
        QCA_OUI_NC_VERSION
    };
    /* preserving efrm pointer, if no sub element is present,
        Skip adding this element */
    efrm = frm;
     /* reserving 2 bytes for the element id and element len*/
    frm += 2;

    OS_MEMCPY(frm, oui, IEEE80211_N(oui));
    frm += IEEE80211_N(oui);

    frm = add_chan_switch_ie(frm, next_channel, ic->ic_chan_switch_cnt);

    frm = add_sec_chan_offset_ie(frm, next_channel);

    chwidth = ieee80211_get_chan_width(next_channel);

    if(IEEE80211_IS_CHAN_11AC(next_channel) && chwidth != CHWIDTH_VHT20) {
        /*If channel width not 20 then add Wideband and txpwr evlp element*/
        frm = ieee80211_add_vht_wide_bw_switch(frm, ni, ic, subtype, 1);
    }
    else {
        widebw = (struct ieee80211_ie_wide_bw_switch *)frm;
        OS_MEMSET(widebw, 0, sizeof(struct ieee80211_ie_wide_bw_switch));
        widebw->elem_id = QCA_UNHANDLED_SUB_ELEM_ID;
        widebw->elem_len = widebw_len - 2;
        frm += widebw_len;
    }

    /* If frame is filled with sub elements then add element id and len*/
    if((frm-2) != efrm)
    {
       ie_len = frm - efrm - 2;
       *efrm++ = IEEE80211_ELEMID_VENDOR;
       *efrm = ie_len;
       /* updating efrm with actual index*/
       efrm = frm;
    }
    return efrm;
}
/*
 * Add an Atheros extended capability information element to a frame
 */
u_int8_t *
ieee80211_add_athextcap(u_int8_t *frm, u_int16_t ath_extcap, u_int8_t weptkipaggr_rxdelim)
{
    static const u_int8_t oui[6] = {(ATH_OUI & 0xff),
                                        ((ATH_OUI >>8) & 0xff),
                                        ((ATH_OUI >> 16) & 0xff),
                                        ATH_OUI_EXTCAP_TYPE,
                                        ATH_OUI_EXTCAP_SUBTYPE,
                                        ATH_OUI_EXTCAP_VERSION};

    *frm++ = IEEE80211_ELEMID_VENDOR;
    *frm++ = 10;
    OS_MEMCPY(frm, oui, sizeof(oui));
    frm += sizeof(oui);
    *frm++ = ath_extcap & 0xff;
    *frm++ = (ath_extcap >> 8) & 0xff;
    *frm++ = weptkipaggr_rxdelim & 0xff;
    *frm++ = 0; /* reserved */
    return frm;
}
/*
 * Add 802.11h information elements to a frame.
 */
u_int8_t *
ieee80211_add_doth(u_int8_t *frm, struct ieee80211vap *vap)
{
    struct ieee80211_ath_channel *c;
    int    i, j, chancnt;
    u_int8_t *chanlist;
    u_int8_t prevchan;
    u_int8_t *frmbeg;
    struct ieee80211com *ic = vap->iv_ic;

    /* XXX ie structures */
    /*
     * Power Capability IE
     */
    chanlist = OS_MALLOC(ic->ic_osdev, sizeof(u_int8_t) * (IEEE80211_CHAN_MAX + 1), 0);
    if (chanlist == NULL) {
        printk("%s[%d] chanlist is null  \n",__func__,__LINE__);
        return frm;
    }
    *frm++ = IEEE80211_ELEMID_PWRCAP;
    *frm++ = 2;
    *frm++ = vap->iv_bsschan->ic_minpower;
    *frm++ = vap->iv_bsschan->ic_maxpower;

	/*
	 * Supported Channels IE as per 802.11h-2003.
	 */
    frmbeg = frm;
    prevchan = 0;
    chancnt = 0;


    for (i = 0; i < ic->ic_nchans; i++)
    {
        c = &ic->ic_channels[i];

        /* Skip turbo channels */
        if (IEEE80211_IS_CHAN_TURBO(c))
            continue;

        /* Skip half/quarter rate channels */
        if (IEEE80211_IS_CHAN_HALF(c) || IEEE80211_IS_CHAN_QUARTER(c))
            continue;

        /* Skip previously reported channels */
        for (j=0; j < chancnt; j++) {
            if (c->ic_ieee == chanlist[j])
                break;
		}
        if (j != chancnt) /* found a match */
            continue;

        chanlist[chancnt] = c->ic_ieee;
        chancnt++;

        if ((c->ic_ieee > prevchan) && prevchan) {
            frm[1] = frm[1] + 1;
        } else {
            frm += 2;
            frm[0] =  c->ic_ieee;
            frm[1] = 1;
        }

        prevchan = c->ic_ieee;
    }

    frm += 2;

    if (chancnt) {
        frmbeg[0] = IEEE80211_ELEMID_SUPPCHAN;
        frmbeg[1] = (u_int8_t)(frm - frmbeg - 2);
    } else {
        frm = frmbeg;
    }

    OS_FREE(chanlist);
    return frm;
}

/*
 * Add ht supported rates to HT element.
 * Precondition: the Rx MCS bitmask is zero'd out.
 */
static void
ieee80211_set_htrates(struct ieee80211vap *vap, u_int8_t *rx_mcs, struct ieee80211com *ic)
{
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap),
             rx_streams = ieee80211_get_rxstreams(ic, vap);

    if (tx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        tx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    if (rx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        rx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    /* First, clear Supported MCS fields. Default to max 1 tx spatial stream */
    rx_mcs[IEEE80211_TX_MCS_OFFSET] &= ~IEEE80211_TX_MCS_SET;

    /* Set Tx MCS Set Defined */
    rx_mcs[IEEE80211_TX_MCS_OFFSET] |= IEEE80211_TX_MCS_SET_DEFINED;

    if (tx_streams != rx_streams) {
        /* Tx MCS Set != Rx MCS Set */
        rx_mcs[IEEE80211_TX_MCS_OFFSET] |= IEEE80211_TX_RX_MCS_SET_NOT_EQUAL;

        switch(tx_streams) {
        case 2:
            rx_mcs[IEEE80211_TX_MCS_OFFSET] |= IEEE80211_TX_2_SPATIAL_STREAMS;
            break;
        case 3:
            rx_mcs[IEEE80211_TX_MCS_OFFSET] |= IEEE80211_TX_3_SPATIAL_STREAMS;
            break;
        case 4:
            rx_mcs[IEEE80211_TX_MCS_OFFSET] |= IEEE80211_TX_4_SPATIAL_STREAMS;
            break;
        }
    }

    /* REVISIT: update bitmask if/when going to > 3 streams */
    switch (rx_streams) {
    default:
        /* Default to single stream */
    case 1:
        /* Advertise all single spatial stream (0-7) mcs rates */
        rx_mcs[IEEE80211_RX_MCS_1_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        break;
    case 2:
        /* Advertise all single & dual spatial stream mcs rates (0-15) */
        rx_mcs[IEEE80211_RX_MCS_1_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        rx_mcs[IEEE80211_RX_MCS_2_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        break;
    case 3:
        /* Advertise all single, dual & triple spatial stream mcs rates (0-23) */
        rx_mcs[IEEE80211_RX_MCS_1_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        rx_mcs[IEEE80211_RX_MCS_2_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        rx_mcs[IEEE80211_RX_MCS_3_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        break;
    case 4:
        rx_mcs[IEEE80211_RX_MCS_1_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        rx_mcs[IEEE80211_RX_MCS_2_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        rx_mcs[IEEE80211_RX_MCS_3_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        rx_mcs[IEEE80211_RX_MCS_4_STREAM_BYTE_OFFSET] = IEEE80211_RX_MCS_ALL_NSTREAM_RATES;
        break;
    }
    if(vap->iv_disable_htmcs) {
        rx_mcs[0] &= ~vap->iv_disabled_ht_mcsset[0];
        rx_mcs[1] &= ~vap->iv_disabled_ht_mcsset[1];
        rx_mcs[2] &= ~vap->iv_disabled_ht_mcsset[2];
        rx_mcs[3] &= ~vap->iv_disabled_ht_mcsset[3];
    }
}

/*
 * Add ht basic rates to HT element.
 */
static void
ieee80211_set_basic_htrates(u_int8_t *frm, const struct ieee80211_rateset *rs)
{
    int i;
    int nrates;

    nrates = rs->rs_nrates;
    if (nrates > IEEE80211_HT_RATE_SIZE)
        nrates = IEEE80211_HT_RATE_SIZE;

    /* set the mcs bit mask from the rates */
    for (i=0; i < nrates; i++) {
        if ((i < IEEE80211_RATE_MAXSIZE) &&
            (rs->rs_rates[i] & IEEE80211_RATE_BASIC))
            *(frm + IEEE80211_RV(rs->rs_rates[i]) / 8) |= 1 << (IEEE80211_RV(rs->rs_rates[i]) % 8);
    }
}

/*
 * Add 802.11n HT Capabilities IE
 */
static void
ieee80211_add_htcap_cmn(struct ieee80211_node *ni, struct ieee80211_ie_htcap_cmn *ie, u_int8_t subtype)
{
    struct ieee80211com       *ic = ni->ni_ic;
    struct ieee80211vap       *vap = ni->ni_vap;
    u_int16_t                 htcap, hc_extcap = 0;
    u_int8_t                  noht40 = 0;
    u_int8_t rx_streams = ieee80211_get_rxstreams(ic, vap);
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap);

    if (rx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        rx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    if (tx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        tx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    /*
     * XXX : Temporarily overide the shortgi based on the htflags,
     * fix this later
     */
    htcap = ic->ic_htcap;
    htcap &= (((vap->iv_htflags & IEEE80211_HTF_SHORTGI40) && vap->iv_sgi) ?
                     ic->ic_htcap  : ~IEEE80211_HTCAP_C_SHORTGI40);
    htcap &= (((vap->iv_htflags & IEEE80211_HTF_SHORTGI20) && vap->iv_sgi) ?
                     ic->ic_htcap  : ~IEEE80211_HTCAP_C_SHORTGI20);

    htcap &= (((vap->vdev_mlme->proto.generic.ldpc & IEEE80211_HTCAP_C_LDPC_RX) &&
              (ieee80211com_get_ldpccap(ic) & IEEE80211_HTCAP_C_LDPC_RX)) ?
              ic->ic_htcap : ~IEEE80211_HTCAP_C_ADVCODING);
    /*
     * Adjust the TX and RX STBC fields based on the chainmask and configuration
     */
    htcap &= (((vap->iv_tx_stbc) && (tx_streams > 1)) ? ic->ic_htcap : ~IEEE80211_HTCAP_C_TXSTBC);
    htcap &= (((vap->iv_rx_stbc) && (rx_streams > 0)) ? ic->ic_htcap : ~IEEE80211_HTCAP_C_RXSTBC);

    /* If bss/regulatory does not allow HT40, turn off HT40 capability */
    if (!(vap->iv_sta_max_ch_cap &&
          get_chwidth_phymode(vap->iv_des_mode)) &&
        !(IEEE80211_IS_CHAN_11N_HT40(vap->iv_bsschan)) &&
        !(IEEE80211_IS_CHAN_11AC_VHT40(vap->iv_bsschan)) &&
        !(IEEE80211_IS_CHAN_11AC_VHT80(vap->iv_bsschan)) &&
        !(IEEE80211_IS_CHAN_11AC_VHT160(vap->iv_bsschan)) &&
        !(IEEE80211_IS_CHAN_11AC_VHT80_80(vap->iv_bsschan))&&
        !(IEEE80211_IS_CHAN_11AX_HE40(vap->iv_bsschan)) &&
        !(IEEE80211_IS_CHAN_11AXA_HE80(vap->iv_bsschan)) &&
        !(IEEE80211_IS_CHAN_11AXA_HE160(vap->iv_bsschan)) &&
        !(IEEE80211_IS_CHAN_11AXA_HE80_80(vap->iv_bsschan))) {
        noht40 = 1;

        /* Don't advertize any HT40 Channel width capability bit */
        /* Forcing sta mode to interop with 11N HT40 only AP removed*/
            htcap &= ~IEEE80211_HTCAP_C_CHWIDTH40;
    }

    if (IEEE80211_IS_CHAN_11NA(vap->iv_bsschan) ||
        IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
        IEEE80211_IS_CHAN_11AXA(vap->iv_bsschan)) {
        htcap &= ~IEEE80211_HTCAP_C_DSSSCCK40;
    }

    /* Should we advertize HT40 capability on 2.4GHz channels? */
    if (IEEE80211_IS_CHAN_11NG(vap->iv_bsschan) ||
        IEEE80211_IS_CHAN_11AXG(vap->iv_bsschan)) {
        if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
            noht40 = 1;
            htcap &= ~IEEE80211_HTCAP_C_CHWIDTH40;
        } else if (vap->iv_opmode == IEEE80211_M_STA) {
            if (!ic->ic_enable2GHzHt40Cap) {
                noht40 = 1;
                htcap &= ~IEEE80211_HTCAP_C_CHWIDTH40;
            }
        } else {
            if (!ic->ic_enable2GHzHt40Cap) {
                noht40 = 1;
                htcap &= ~IEEE80211_HTCAP_C_CHWIDTH40;
            } else if (!(ni->ni_htcap & IEEE80211_HTCAP_C_CHWIDTH40)) {
                noht40 = 1;
            }
        }
    }

    if (noht40) {
        /* Don't advertize any HT40 capability bits */
        htcap &= ~(IEEE80211_HTCAP_C_DSSSCCK40 |
                   IEEE80211_HTCAP_C_SHORTGI40);
    }


    if (!ieee80211_vap_dynamic_mimo_ps_is_set(ni->ni_vap)) {
        /* Don't advertise Dynamic MIMO power save if not configured */
        htcap &= ~IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC;
        htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED;
    }

    /* Set support for 20/40 Coexistence Management frame support */
    htcap |= (vap->iv_ht40_intolerant) ? IEEE80211_HTCAP_C_INTOLERANT40 : 0;

    ie->hc_cap = htole16(htcap);

    ie->hc_maxampdu	= ic->ic_maxampdu;
#if ATH_SUPPORT_WAPI
    if(IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
       && (RSN_CIPHER_IS_SMS4(&vap->iv_rsn))) {
#else
	&& wlan_crypto_vdev_has_ucastcipher(vap->vdev_obj,
                           (1 << WLAN_CRYPTO_CIPHER_WAPI_SMS4))) {
#endif
        ie->hc_mpdudensity = 7;
    } else
#endif
    {
        /* MPDU Density : If User provided mpdudensity,
         * Take user configured value*/
        if(ic->ic_mpdudensityoverride) {
            ie->hc_mpdudensity = ic->ic_mpdudensityoverride >> 1;
        } else {
            /* WAR for MPDU DENSITY : In Beacon frame the mpdu density is set as zero. In association response and request if the peer txstreams
             * is 4 set the MPDU density to 16.
             */

           if (ic->ic_is_mode_offload(ic) && ic->ic_is_target_ar900b(ic) &&
                    (IEEE80211_IS_CHAN_5GHZ(vap->iv_bsschan)) &&
                    ((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) || (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ)) &&
                    (MIN(ni->ni_txstreams, rx_streams) == IEEE80211_MAX_SPATIAL_STREAMS)) {
                ie->hc_mpdudensity = IEEE80211_HTCAP_MPDUDENSITY_16;
            } else {
                ie->hc_mpdudensity = ic->ic_mpdudensity;
            }
        }

    }
    ie->hc_reserved	= 0;

    /* Initialize the MCS bitmask */
    OS_MEMZERO(ie->hc_mcsset, sizeof(ie->hc_mcsset));

    /* Set supported MCS set */
    ieee80211_set_htrates(vap, ie->hc_mcsset, ic);
    if(IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
            && (RSN_CIPHER_IS_WEP(&vap->iv_rsn)
             || (RSN_CIPHER_IS_TKIP(&vap->iv_rsn) && (!RSN_CIPHER_IS_CCMP128(&vap->iv_rsn)) &&
                 (!RSN_CIPHER_IS_CCMP256(&vap->iv_rsn)) &&
                 (!RSN_CIPHER_IS_GCMP128(&vap->iv_rsn)) &&
                 (!RSN_CIPHER_IS_GCMP256(&vap->iv_rsn))))) {
#else
      && wlan_crypto_is_htallowed(vap->vdev_obj, NULL)) {
#endif
        /*
         * WAR for Tx FIFO underruns with MCS15 in WEP mode. Exclude
         * MCS15 from rates if WEP encryption is set in HT20 mode
         */
        if (IEEE80211_IS_CHAN_11N_HT20(vap->iv_bsschan))
            ie->hc_mcsset[IEEE80211_RX_MCS_2_STREAM_BYTE_OFFSET] &= 0x7F;
    }

#if ATH_SUPPORT_WRAP
    if ((vap->iv_psta == 1)) {
        if ((ic->ic_proxystarxwar == 1) &&
            IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) &&
                IEEE80211_IS_CHAN_11N_HT40(vap->iv_bsschan)
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                    && (RSN_CIPHER_IS_TKIP(&vap->iv_rsn) || (RSN_CIPHER_IS_CCMP128(&vap->iv_rsn)) ||
                     (RSN_CIPHER_IS_CCMP256(&vap->iv_rsn)) || (RSN_CIPHER_IS_GCMP128(&vap->iv_rsn)) ||
                     (RSN_CIPHER_IS_GCMP256(&vap->iv_rsn)) )) {
#else
	&& wlan_crypto_vdev_has_ucastcipher(vap->vdev_obj,
                           ((1 << WLAN_CRYPTO_CIPHER_TKIP)
                             | (1 << WLAN_CRYPTO_CIPHER_AES_CCM)
                             | (1 << WLAN_CRYPTO_CIPHER_AES_GCM)
                             | (1 << WLAN_CRYPTO_CIPHER_AES_CCM_256)
                             | (1 << WLAN_CRYPTO_CIPHER_AES_GCM_256)))) {
#endif
                ie->hc_mcsset[IEEE80211_RX_MCS_3_STREAM_BYTE_OFFSET] &= 0x7F;
        }
    }
#endif

#ifdef ATH_SUPPORT_TxBF
    ic->ic_set_txbf_caps(ic);       /* update txbf cap*/
    ie->hc_txbf.value = htole32(ic->ic_txbf.value);

    /* disable TxBF mode for SoftAP mode of win7*/
    if (vap->iv_opmode == IEEE80211_M_HOSTAP){
        if(vap->iv_txbfmode == 0 ){
            ie->hc_txbf.value = 0;
        }
    }
#if ATH_SUPPORT_WRAP
    /* disable TxBF mode for WRAP in case of Direct Attach only*/
    if(!ic->ic_is_mode_offload(ic)) {
            if (vap->iv_wrap || vap->iv_psta)
                    ie->hc_txbf.value = 0;
    }
#endif

#if ATH_SUPPORT_WAPI
    /* disable TxBF mode for WAPI on Direct Attach chips */
    if( !ic->ic_is_mode_offload(ic) && ieee80211_vap_wapi_is_set(vap) )
        ie->hc_txbf.value = 0;
#endif

    if (ie->hc_txbf.value!=0) {
        hc_extcap |= IEEE80211_HTCAP_EXTC_HTC_SUPPORT;    /*enable +HTC support*/
    }
#else
    ie->hc_txbf    = 0;
#endif
    ie->hc_extcap  = htole16(hc_extcap);
    ie->hc_antenna = 0;
}

u_int8_t *
ieee80211_add_htcap(u_int8_t *frm, struct ieee80211_node *ni, u_int8_t subtype)
{
    struct ieee80211_ie_htcap_cmn *ie;
    int htcaplen;
    struct ieee80211_ie_htcap *htcap = (struct ieee80211_ie_htcap *)frm;

    htcap->hc_id      = IEEE80211_ELEMID_HTCAP_ANA;
    htcap->hc_len     = sizeof(struct ieee80211_ie_htcap) - 2;

    ie = &htcap->hc_ie;
    htcaplen = sizeof(struct ieee80211_ie_htcap);

    ieee80211_add_htcap_cmn(ni, ie, subtype);

    return frm + htcaplen;
}

u_int8_t *
ieee80211_add_htcap_vendor_specific(u_int8_t *frm, struct ieee80211_node *ni,u_int8_t subtype)
{
    struct ieee80211_ie_htcap_cmn *ie;
    int htcaplen;
    struct vendor_ie_htcap *htcap = (struct vendor_ie_htcap *)frm;

    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_DEBUG, "%s: use HT caps IE vendor specific\n",
                      __func__);

    htcap->hc_id      = IEEE80211_ELEMID_VENDOR;
    htcap->hc_oui[0]  = (ATH_HTOUI >> 16) & 0xff;
    htcap->hc_oui[1]  = (ATH_HTOUI >>  8) & 0xff;
    htcap->hc_oui[2]  = ATH_HTOUI & 0xff;
    htcap->hc_ouitype = IEEE80211_ELEMID_HTCAP_VENDOR;
    htcap->hc_len     = sizeof(struct vendor_ie_htcap) - 2;

    ie = &htcap->hc_ie;
    htcaplen = sizeof(struct vendor_ie_htcap);

    ieee80211_add_htcap_cmn(ni, ie,subtype);

    return frm + htcaplen;
}

/*
 * Add 802.11n HT Information IE
 */
/* NB: todo: still need to handle the case for when there may be non-HT STA's on channel (extension
   and/or control) that are not a part of the BSS.  Process beacons for no HT IEs and
   process assoc-req for BS' other than our own */
void
ieee80211_update_htinfo_cmn(struct ieee80211_ie_htinfo_cmn *ie, struct ieee80211_node *ni)
{
    struct ieee80211com        *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    u_int8_t chwidth = 0;
    struct ieee80211_bwnss_map nssmap;
    u_int8_t rx_chainmask = ieee80211com_get_rx_chainmask(ic);

    qdf_mem_zero(&nssmap, sizeof(nssmap));

    /*
     ** If the value in the VAP is set, we use that instead of the actual setting
     ** per Srini D.  Hopefully this matches the actual setting.
     */
    if( vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
        chwidth = vap->iv_chwidth;
    } else {
        chwidth = ic_cw_width;
    }
    ie->hi_txchwidth = (chwidth == IEEE80211_CWM_WIDTH20) ?
        IEEE80211_HTINFO_TXWIDTH_20 : IEEE80211_HTINFO_TXWIDTH_2040;

    /*
     ** If the value in the VAP for the offset is set, use that per
     ** Srini D.  Otherwise, use the actual setting
     */

    if( vap->iv_chextoffset != 0 ) {
        switch( vap->iv_chextoffset ) {
            case 1:
                ie->hi_extchoff = IEEE80211_HTINFO_EXTOFFSET_NA;
                break;
            case 2:
                ie->hi_extchoff =  IEEE80211_HTINFO_EXTOFFSET_ABOVE;
                break;
            case 3:
                ie->hi_extchoff =  IEEE80211_HTINFO_EXTOFFSET_BELOW;
                break;
            default:
                break;
        }
    } else {
        if ((ic_cw_width == IEEE80211_CWM_WIDTH40)||(ic_cw_width == IEEE80211_CWM_WIDTH80) || (ic_cw_width == IEEE80211_CWM_WIDTH160)) {
            switch (ic->ic_cwm_get_extoffset(ic)) {
                case 1:
                    ie->hi_extchoff = IEEE80211_HTINFO_EXTOFFSET_ABOVE;
                    break;
                case -1:
                    ie->hi_extchoff = IEEE80211_HTINFO_EXTOFFSET_BELOW;
                    break;
                case 0:
                default:
                    ie->hi_extchoff = IEEE80211_HTINFO_EXTOFFSET_NA;
            }
        } else {
            ie->hi_extchoff = IEEE80211_HTINFO_EXTOFFSET_NA;
        }
    }
    if (vap->iv_disable_HTProtection) {
        /* Force HT40: no HT protection*/
        ie->hi_opmode = IEEE80211_HTINFO_OPMODE_PURE;
        ie->hi_obssnonhtpresent=IEEE80211_HTINFO_OBSS_NONHT_NOT_PRESENT;
        ie->hi_rifsmode = IEEE80211_HTINFO_RIFSMODE_ALLOWED;
    }
    else if (ic->ic_sta_assoc > ic->ic_ht_sta_assoc) {
        /*
         * Legacy stations associated.
         */
        ie->hi_opmode =IEEE80211_HTINFO_OPMODE_MIXED_PROT_ALL;
        ie->hi_obssnonhtpresent = IEEE80211_HTINFO_OBSS_NONHT_PRESENT;
        ie->hi_rifsmode	= IEEE80211_HTINFO_RIFSMODE_PROHIBITED;
    }
    else if (ieee80211_ic_non_ht_ap_is_set(ic)) {
        /*
         * Overlapping with legacy BSSs.
         */
        ie->hi_opmode = IEEE80211_HTINFO_OPMODE_MIXED_PROT_OPT;
        ie->hi_obssnonhtpresent =IEEE80211_HTINFO_OBSS_NONHT_NOT_PRESENT;
        ie->hi_rifsmode	= IEEE80211_HTINFO_RIFSMODE_PROHIBITED;
    }
    else if (ie->hi_txchwidth == IEEE80211_HTINFO_TXWIDTH_2040 && ic->ic_ht_sta_assoc > ic->ic_ht40_sta_assoc) {
        /*
         * HT20 Stations present in HT40 BSS.
         */
        ie->hi_opmode = IEEE80211_HTINFO_OPMODE_MIXED_PROT_40;
        ie->hi_obssnonhtpresent = IEEE80211_HTINFO_OBSS_NONHT_NOT_PRESENT;
        ie->hi_rifsmode	= IEEE80211_HTINFO_RIFSMODE_ALLOWED;
    } else {
        /*
         * all Stations are HT40 capable
         */
        ie->hi_opmode = IEEE80211_HTINFO_OPMODE_PURE;
        ie->hi_obssnonhtpresent=IEEE80211_HTINFO_OBSS_NONHT_NOT_PRESENT;
        ie->hi_rifsmode	= IEEE80211_HTINFO_RIFSMODE_ALLOWED;
    }

    if (((IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
          IEEE80211_IS_CHAN_11NG(vap->iv_bsschan)) &&
          ieee80211vap_vhtallowed(vap)) ||
         (IEEE80211_IS_CHAN_HE(vap->iv_bsschan) &&
          ieee80211vap_heallowed(vap))) {

        /*
         *All VHT/HE AP should have RIFS bit set to 0
         */
        ie->hi_rifsmode = IEEE80211_HTINFO_RIFSMODE_PROHIBITED;
    }

    if (vap->iv_opmode != IEEE80211_M_IBSS &&
                ic->ic_ht_sta_assoc > ic->ic_ht_gf_sta_assoc)
        ie->hi_nongfpresent = 1;
    else
        ie->hi_nongfpresent = 0;

    if (!ieee80211_get_bw_nss_mapping(vap, &nssmap, rx_chainmask)) {
        if ((ic_cw_width == IEEE80211_CWM_WIDTH160) && vap->iv_ext_nss_support &&
                 (!(nssmap.flag == IEEE80211_NSSMAP_SAME_NSS_FOR_ALL_BW))) {
            HTINFO_CCFS2_SET(vap->iv_bsschan->ic_vhtop_ch_freq_seg2, ie);
        }
    }

    if (vap->iv_csa_interop_bss_active) {
        ie->hi_opmode = IEEE80211_HTINFO_OPMODE_MIXED_PROT_ALL;
        ie->hi_extchoff = IEEE80211_HTINFO_EXTOFFSET_NA;
        ie->hi_txchwidth = IEEE80211_CWM_WIDTH20;
    }
}

static void
ieee80211_add_htinfo_cmn(struct ieee80211_node *ni, struct ieee80211_ie_htinfo_cmn *ie)
{
    struct ieee80211com        *ic = ni->ni_ic;
    struct ieee80211vap        *vap = ni->ni_vap;

    OS_MEMZERO(ie, sizeof(struct ieee80211_ie_htinfo_cmn));

    /* set control channel center in IE */
    ie->hi_ctrlchannel 	= ieee80211_chan2ieee(ic, vap->iv_bsschan);

    ieee80211_update_htinfo_cmn(ie,ni);
    /* Set the basic MCS Set */
    OS_MEMZERO(ie->hi_basicmcsset, sizeof(ie->hi_basicmcsset));
    ieee80211_set_basic_htrates(ie->hi_basicmcsset, &ni->ni_htrates);

    ieee80211_update_htinfo_cmn(ie, ni);
}

u_int8_t *
ieee80211_add_htinfo(u_int8_t *frm, struct ieee80211_node *ni)
{
    struct ieee80211_ie_htinfo_cmn *ie;
    int htinfolen;
    struct ieee80211_ie_htinfo *htinfo = (struct ieee80211_ie_htinfo *)frm;

    htinfo->hi_id      = IEEE80211_ELEMID_HTINFO_ANA;
    htinfo->hi_len     = sizeof(struct ieee80211_ie_htinfo) - 2;

    ie = &htinfo->hi_ie;
    htinfolen = sizeof(struct ieee80211_ie_htinfo);

    ieee80211_add_htinfo_cmn(ni, ie);

    return frm + htinfolen;
}

u_int8_t *
ieee80211_add_htinfo_vendor_specific(u_int8_t *frm, struct ieee80211_node *ni)
{
    struct ieee80211_ie_htinfo_cmn *ie;
    int htinfolen;
    struct vendor_ie_htinfo *htinfo = (struct vendor_ie_htinfo *) frm;

    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_DEBUG, "%s: use HT info IE vendor specific\n",
                      __func__);

    htinfo->hi_id      = IEEE80211_ELEMID_VENDOR;
    htinfo->hi_oui[0]  = (ATH_HTOUI >> 16) & 0xff;
    htinfo->hi_oui[1]  = (ATH_HTOUI >>  8) & 0xff;
    htinfo->hi_oui[2]  = ATH_HTOUI & 0xff;
    htinfo->hi_ouitype = IEEE80211_ELEMID_HTINFO_VENDOR;
    htinfo->hi_len     = sizeof(struct vendor_ie_htinfo) - 2;

    ie = &htinfo->hi_ie;
    htinfolen = sizeof(struct vendor_ie_htinfo);

    ieee80211_add_htinfo_cmn(ni, ie);

    return frm + htinfolen;
}

static inline void ieee80211_add_twt_extcap(struct ieee80211_node *ni,
                                            uint8_t *ext_capflags4,
                                            uint8_t subtype)
{
    struct ieee80211com *ic  = ni->ni_ic;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
#if QCN_IE
    bool is_he_peer =  (IEEE80211_IS_CHAN_11AX(ic->ic_curchan)
                        && ieee80211vap_heallowed(ni->ni_vap)
                        && (ni->ni_ext_flags & IEEE80211_NODE_HE));
#endif

    pdev = ic->ic_pdev_obj;
    if (pdev == NULL) {
        qdf_err("Invalid pdev in ic");
        return;
    }
    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL) {
        qdf_err("Invalid psoc in pdev");
        return;
    }

#if QCN_IE
    if (((subtype != IEEE80211_FC0_SUBTYPE_ASSOC_RESP)
                || ni->ni_qcn_ie || is_he_peer) &&
        (wlan_psoc_nif_fw_ext_cap_get(psoc, WLAN_SOC_CEXT_TWT_REQUESTER)))
        *ext_capflags4 |= IEEE80211_EXTCAPIE_TWT_REQ;

    if (((subtype != IEEE80211_FC0_SUBTYPE_ASSOC_RESP)
                || ni->ni_qcn_ie || is_he_peer) &&
        (wlan_psoc_nif_fw_ext_cap_get(psoc, WLAN_SOC_CEXT_TWT_RESPONDER)) &&
        (ni->ni_vap->iv_twt_rsp))
        *ext_capflags4 |= IEEE80211_EXTCAPIE_TWT_RESP;
#endif
}

static inline void ieee80211_add_obss_narrow_bw_ru(struct ieee80211_node *ni,
                                                   uint8_t *ext_capflags4,
                                                   uint8_t subtype)
{
    struct ieee80211com *ic       = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;

    if ((ni == NULL) || (ni->ni_ic == NULL)) {
        (!ni) ? qdf_err("Invalid ni") : qdf_err("Invalid ni_ic");
        return;
    }
    ic = ni->ni_ic;

    pdev = ic->ic_pdev_obj;
    if (pdev == NULL) {
        qdf_err("Invalid pdev in ic");
        return;
    }
    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL) {
        qdf_err("Invalid psoc in pdev");
        return;
    }

    /* Flag is set only for beacons and probe responses and flag is supported
     * for 802.11ax only */
    if (IEEE80211_IS_CHAN_11AX(ic->ic_curchan) &&
        ((subtype == IEEE80211_FC0_SUBTYPE_BEACON) ||
        (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)) &&
        (wlan_psoc_nif_fw_ext_cap_get(psoc, WLAN_SOC_CEXT_OBSS_NBW_RU))) {
        *ext_capflags4 |= IEEE80211_EXTCAPIE_OBSS_NBW_RU;
    }

}
/*
 * Add ext cap element.
 */
u_int8_t *
ieee80211_add_extcap(u_int8_t *frm,struct ieee80211_node *ni, uint8_t subtype)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_ie_ext_cap *ie = (struct ieee80211_ie_ext_cap *) frm;
    u_int32_t ext_capflags = 0;
    u_int32_t ext_capflags2 = 0;
    u_int8_t ext_capflags3 = 0;
    u_int8_t ext_capflags4 = 0;
    u_int8_t ext_capflags5 = 0;
    u_int32_t ie_elem_len = 0;
    u_int8_t fils_en = 0;

    if (!(ic->ic_flags & IEEE80211_F_COEXT_DISABLE)) {
        ext_capflags |= IEEE80211_EXTCAPIE_2040COEXTMGMT;
    }
    ieee80211_wnm_add_extcap(ni, &ext_capflags);

#if UMAC_SUPPORT_PROXY_ARP
    if (ieee80211_vap_proxyarp_is_set(vap) &&
            vap->iv_opmode == IEEE80211_M_HOSTAP)
    {
        ext_capflags |= IEEE80211_EXTCAPIE_PROXYARP;
    }
#endif
#if ATH_SUPPORT_HS20
    if (vap->iv_hotspot_xcaps) {
        ext_capflags |= vap->iv_hotspot_xcaps;
    }
    if (vap->iv_hotspot_xcaps2) {
        ext_capflags2 |= vap->iv_hotspot_xcaps2;
    }
#endif
    if (vap->rtt_enable) {
        /* Date: 6/5/201. we may need seperate control of
           these bits. Setting these bits will follow
           evolution of 802.11mc spec */
        ext_capflags3 |= IEEE80211_EXTCAPIE_FTM_RES;
        ext_capflags3 |= IEEE80211_EXTCAPIE_FTM_INIT;
    }

    if (vap->lcr_enable) {
        ext_capflags |= IEEE80211_EXTCAPIE_CIVLOC;
    }

    if (vap->lci_enable) {
        ext_capflags |= IEEE80211_EXTCAPIE_GEOLOC;
    }

    if (vap->iv_enable_ecsaie) {
        ext_capflags |= IEEE80211_EXTCAPIE_ECSA;
    }

    if(wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                  WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        ext_capflags |= IEEE80211_EXTCAPIE_MBSSID;
        /* support sending complete list of NonTxBSSID profiles */
        ext_capflags5 |= IEEE80211_EXTCAPIE_COMPL_LIST_MBSS;
    }

    /* Support reception of Operating Mode notification */
    ext_capflags2 |= IEEE80211_EXTCAPIE_OP_MODE_NOTIFY;

#if WLAN_SUPPORT_FILS
    fils_en = wlan_fils_is_enable(vap->vdev_obj);
#endif
    if(fils_en) {
        ext_capflags4 |= IEEE80211_EXTCAPIE_FILS;
    }

    ieee80211_add_twt_extcap(ni, &ext_capflags4, subtype);
    ieee80211_add_obss_narrow_bw_ru(ni, &ext_capflags4, subtype);

    if (ext_capflags || ext_capflags2 || (ext_capflags3) ||
       (ext_capflags4) || (ext_capflags5)) {

        if (ext_capflags5) {
            ie_elem_len = sizeof(struct ieee80211_ie_ext_cap);
        } else if (ext_capflags4) {
            ie_elem_len = sizeof(struct ieee80211_ie_ext_cap) - sizeof(ie->ext_capflags5);
        } else if (ext_capflags3) {
            ie_elem_len = sizeof(struct ieee80211_ie_ext_cap) -
                          (sizeof(ie->ext_capflags4) + sizeof(ie->ext_capflags5));
        } else {
            ie_elem_len = sizeof(struct ieee80211_ie_ext_cap) -
                    (sizeof(ie->ext_capflags3) + sizeof(ie->ext_capflags4) +
                     sizeof(ie->ext_capflags5));
        }

        OS_MEMSET(ie, 0, sizeof(struct ieee80211_ie_ext_cap));
        ie->elem_id = IEEE80211_ELEMID_XCAPS;
        ie->elem_len = ie_elem_len - 2;
        ie->ext_capflags = htole32(ext_capflags);
        ie->ext_capflags2 = htole32(ext_capflags2);
        ie->ext_capflags3 = ext_capflags3;
        ie->ext_capflags4 = ext_capflags4;
        ie->ext_capflags5 = ext_capflags5;
        return (frm + ie_elem_len);
    }
    else {
        return frm;
    }
}

void ieee80211_parse_extcap(struct ieee80211_node *ni, uint8_t *ie)
{
    struct ieee80211_ie_ext_cap *extcap = (struct ieee80211_ie_ext_cap *) ie;
    uint8_t len = extcap->elem_len;
    uint32_t ext_capflags = 0, ext_capflags2 = 0;
    uint8_t ext_capflags3 = 0, ext_capflags4 = 0;

    if (len >= sizeof(ext_capflags)) {
        ext_capflags = le32toh(extcap->ext_capflags);
        len -= sizeof(ext_capflags);
        if (len >= sizeof(ext_capflags2)) {
            ext_capflags2 = le32toh(extcap->ext_capflags2);
            len -= sizeof(ext_capflags2);
            if (len >= sizeof(ext_capflags3)) {
                ext_capflags3 = extcap->ext_capflags3;
                len -= sizeof(ext_capflags3);
                if (len >= sizeof(ext_capflags4)) {
                    ext_capflags4 = extcap->ext_capflags4;
                    len -= sizeof(ext_capflags4);
                }
            }
        }
    }

#if ATH_SUPPORT_HS20
    if (ext_capflags2 & IEEE80211_EXTCAPIE_QOS_MAP)
        ni->ni_qosmap_enabled = 1;
    else
        ni->ni_qosmap_enabled = 0;

    /* Copy first word only to node structure, only part used at the moment */
    ni->ext_caps.ni_ext_capabilities = ext_capflags;
#endif

    if (ext_capflags4) {
        /* TWT */
#if QCN_IE
        if ((ext_capflags4 & IEEE80211_EXTCAPIE_TWT_REQ) &&
            (ni->ni_qcn_ie))
            ni->ni_ext_flags |= IEEE80211_NODE_TWT_REQUESTER;

        if ((ext_capflags4 & IEEE80211_EXTCAPIE_TWT_RESP) &&
            (ni->ni_qcn_ie))
            ni->ni_ext_flags |= IEEE80211_NODE_TWT_RESPONDER;
#endif
    }
}

#if ATH_SUPPORT_HS20
u_int8_t *ieee80211_add_qosmapset(u_int8_t *frm, struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_qos_map *qos_map = &vap->iv_qos_map;
    struct ieee80211_ie_qos_map_set *ie_qos_map_set =
        (struct ieee80211_ie_qos_map_set *)frm;
    u_int8_t *pos = ie_qos_map_set->qos_map_set;
    u_int8_t len, elem_len = 0;

    if (qos_map->valid && ni->ni_qosmap_enabled) {
        if (qos_map->num_dscp_except) {
            len = qos_map->num_dscp_except *
                  sizeof(struct ieee80211_dscp_exception);
            OS_MEMCPY(pos, qos_map->dscp_exception, len);
            elem_len += len;
            pos += len;
        }

        len = IEEE80211_MAX_QOS_UP_RANGE * sizeof(struct ieee80211_dscp_range);
        OS_MEMCPY(pos, qos_map->up, len);
        elem_len += len;
        pos += len;

        ie_qos_map_set->elem_id = IEEE80211_ELEMID_QOS_MAP;
        ie_qos_map_set->elem_len = elem_len;

        return pos;

    } else {
        /* QoS Map is not valid or not enabled */
        return frm;
    }
}
#endif /* ATH_SUPPORT_HS20 */

/*
 * Update overlapping bss scan element.
 */
void
ieee80211_update_obss_scan(struct ieee80211_ie_obss_scan *ie,
                           struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;

    if ( ie == NULL )
        return;

    ie->scan_interval = (vap->iv_chscaninit) ?
          htole16(vap->iv_chscaninit):htole16(IEEE80211_OBSS_SCAN_INTERVAL_DEF);
}

/*
 * Add overlapping bss scan element.
 */
u_int8_t *
ieee80211_add_obss_scan(u_int8_t *frm, struct ieee80211_node *ni)
{
    struct ieee80211_ie_obss_scan *ie = (struct ieee80211_ie_obss_scan *) frm;

    OS_MEMSET(ie, 0, sizeof(struct ieee80211_ie_obss_scan));
    ie->elem_id = IEEE80211_ELEMID_OBSS_SCAN;
    ie->elem_len = sizeof(struct ieee80211_ie_obss_scan) - 2;
    ieee80211_update_obss_scan(ie, ni);
    ie->scan_passive_dwell = htole16(IEEE80211_OBSS_SCAN_PASSIVE_DWELL_DEF);
    ie->scan_active_dwell = htole16(IEEE80211_OBSS_SCAN_ACTIVE_DWELL_DEF);
    ie->scan_passive_total = htole16(IEEE80211_OBSS_SCAN_PASSIVE_TOTAL_DEF);
    ie->scan_active_total = htole16(IEEE80211_OBSS_SCAN_ACTIVE_TOTAL_DEF);
    ie->scan_thresh = htole16(IEEE80211_OBSS_SCAN_THRESH_DEF);
    ie->scan_delay = htole16(IEEE80211_OBSS_SCAN_DELAY_DEF);
    return frm + sizeof (struct ieee80211_ie_obss_scan);
}

void
ieee80211_add_capability(u_int8_t * frm, struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_rateset *rs = &ni->ni_rates;
    uint16_t capinfo;

    if (vap->iv_opmode == IEEE80211_M_IBSS){
        if(ic->ic_softap_enable)
            capinfo = IEEE80211_CAPINFO_ESS;
        else
            capinfo = IEEE80211_CAPINFO_IBSS;
    } else
        capinfo = IEEE80211_CAPINFO_ESS;

    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
        capinfo |= IEEE80211_CAPINFO_PRIVACY;
    if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
        IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan))
        capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
    if (ic->ic_flags & IEEE80211_F_SHSLOT)
        capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap))
        capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;

    if (IEEE80211_VAP_IS_PUREB_ENABLED(vap)) {
        capinfo &= ~IEEE80211_CAPINFO_SHORT_SLOTTIME;
        rs = &ic->ic_sup_rates[IEEE80211_MODE_11B];
    } else if (IEEE80211_VAP_IS_PUREG_ENABLED(vap)) {
        ieee80211_setpuregbasicrates(rs);
    }

    /* set rrm capbabilities, if supported */
    if (ieee80211_vap_rrm_is_set(vap)) {
        capinfo |= IEEE80211_CAPINFO_RADIOMEAS;
    }

    *(u_int16_t *)frm = htole16(capinfo);
}

uint8_t *
ieee80211_add_security_ie(u_int8_t *frm, struct ieee80211vap *vap, int subtype)
{
    bool add_wpa_ie = true;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
#endif

    if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
      qdf_print("[MBSSID]vap id:%d \n", vap->iv_unit);
    /*
     * check if OS shim has setup RSN IE
     */
    if (!vap->iv_rsn_override) {
        IEEE80211_VAP_LOCK(vap);
        if (vap->iv_app_ie[subtype].length) {
            add_wpa_ie = ieee80211_check_wpaie(vap, vap->iv_app_ie[subtype].ie,
                                               vap->iv_app_ie[subtype].length);
        }
        /*
         * Check if RSN IE is present in the frame. Traverse the linked list
         * and check for RSN IE.
         * If not present, add it in the driver
         */
        add_wpa_ie = ieee80211_mlme_app_ie_check_wpaie(vap,subtype);
        if (vap->iv_opt_ie.length) {
            add_wpa_ie = ieee80211_check_wpaie(vap, vap->iv_opt_ie.ie,
                                               vap->iv_opt_ie.length);
        }
        IEEE80211_VAP_UNLOCK(vap);
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

    if (add_wpa_ie && !vap->iv_rsn_override) {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_WPA))) {
            frm = wlan_crypto_build_wpaie(vap->vdev_obj, frm);
            if (!frm)
                return NULL;
       }
#else
        if (RSN_AUTH_IS_WPA(rsn)) {
    if (subtype == IEEE80211_FC0_SUBTYPE_BEACON)
            frm = ieee80211_setup_wpa_ie(vap, frm);
        }
#endif
    }

#if ATH_SUPPORT_WAPI
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_WAPI)))
#else
    if (RSN_AUTH_IS_WAI(rsn))
#endif
    {
      qdf_print("[MBSSID]line:%d\n",__LINE__);
        frm = ieee80211_setup_wapi_ie(vap, frm);
        if (!frm)
            return NULL;
    }
#endif

    return frm;
}

void
ieee80211_add_app_ie(u_int8_t *frm, struct ieee80211vap *vap, int subtype)
{
    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_opt_ie.length) {
        OS_MEMCPY(frm, vap->iv_opt_ie.ie, vap->iv_opt_ie.length);
        frm += vap->iv_opt_ie.length;
    }

    if (vap->iv_app_ie[subtype].length > 0) {
      //      qdf_print(FL("\n"));
        OS_MEMCPY(frm,vap->iv_app_ie[subtype].ie,
                  vap->iv_app_ie[subtype].length);
        frm += vap->iv_app_ie[subtype].length;
    }

    /* Add the Application IE's */
    frm = ieee80211_mlme_app_ie_append(vap, subtype, frm);
    IEEE80211_VAP_UNLOCK(vap);
}

void
ieee80211_add_he_bsscolor_change_ie(struct ieee80211_beacon_offsets *bo,
                                    wbuf_t wbuf,
                                    struct ieee80211_node *ni,
                                    uint8_t subtype,
                                    int *len_changed) {
    struct ieee80211vap *vap                              = ni->ni_vap;
    struct ieee80211com *ic                               = vap->iv_ic;
    struct ieee80211_ie_hebsscolor_change *hebsscolor_chg =
                    (struct ieee80211_ie_hebsscolor_change *) bo->bo_bcca;
    uint8_t hebsscolor_ie_len                             =
                            sizeof(struct ieee80211_ie_hebsscolor_change);
    uint8_t *tempbuf                                      = NULL;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                                "%s>>", __func__);

    if (hebsscolor_chg->elem_id == IEEE80211_ELEMID_EXTN
        && hebsscolor_chg->elem_id_ext == IEEE80211_ELEMID_EXT_BSSCOLOR_CHG) {
        hebsscolor_chg->color_switch_cntdown =
            ic->ic_he_bsscolor_change_tbtt - vap->iv_he_bsscolor_change_count;
    } else {
        /* Copy out trailer to open up a slot */
        tempbuf = OS_MALLOC(ic->ic_osdev, bo->bo_bcca_trailerlen,
                GFP_KERNEL);
        if(!tempbuf) {
            QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                                    "%s<< tempbuf is NULL", __func__);
            return;
        }

        OS_MEMCPY(tempbuf, bo->bo_bcca, bo->bo_bcca_trailerlen);
        OS_MEMCPY(bo->bo_bcca + hebsscolor_ie_len,
                                tempbuf, bo->bo_bcca_trailerlen);
        OS_FREE(tempbuf);

        hebsscolor_chg->elem_id     = IEEE80211_ELEMID_EXTN;
        hebsscolor_chg->elem_len    = hebsscolor_ie_len - 2;
        hebsscolor_chg->elem_id_ext = IEEE80211_ELEMID_EXT_BSSCOLOR_CHG;
        hebsscolor_chg->color_switch_cntdown =
            ic->ic_he_bsscolor_change_tbtt - vap->iv_he_bsscolor_change_count;
        hebsscolor_chg->new_bss_color = ic->ic_bsscolor_hdl.selected_bsscolor;

        ieee80211_adjust_bos_for_bsscolor_change_ie(bo, hebsscolor_ie_len);

        /* Indicate new beacon length so other layers may
         * manage memory.
         */
        wbuf_append(wbuf, hebsscolor_ie_len);

        /* Indicate new beacon length so other layers may
         * manage memory.
         */
        *len_changed = 1;
    }

    if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
        vap->iv_he_bsscolor_change_count++;

        /* check change_count against change_tbtt + 1
         * so that BCCA with color switch countdown
         * can be completed
         */
        if (vap->iv_he_bsscolor_change_count ==
                ic->ic_he_bsscolor_change_tbtt + 1) {
            vap->iv_he_bsscolor_change_ongoing = false;

            /* check bsscolor change completion for
             * all vaps
             */
            if (!ieee80211_is_bcca_ongoing_for_any_vap(ic)) {
                /* BCCA completed for all vap. Enable
                 * BSS Color in HEOP
                 */
#if SUPPORT_11AX_D3
                ic->ic_he.heop_bsscolor_info &=
                                    ~IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK;
#else
                ic->ic_he.heop_param         &=
                                    ~IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK;
#endif
            }

            if (vap->iv_he_bsscolor_detcn_configd_vap) {
                /* re-configure bss color detection in fw */
                ic->ic_config_bsscolor_offload(vap, false);
            }

            /* remove BCCA ie */
            vap->iv_he_bsscolor_remove_ie = true;
            vap->iv_bcca_ie_status = BCCA_NA;

            /* configure FW with new bss color */
            if (ic->ic_vap_set_param) {
                if (ic->ic_vap_set_param(vap,
                      IEEE80211_CONFIG_HE_BSS_COLOR,
                      ic->ic_bsscolor_hdl.selected_bsscolor)) {
                    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                          "%s: bsscolor update to fw failed for vdev "
                          "id: 0x%x", __func__,
                          OL_ATH_VAP_NET80211(vap)->av_if_id);
                }
            }
        } /* if tbtt */
    } /* if beacon */

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
            "%s<< iv_he_bsscolor_change_count: 0x%x", __func__,
                                vap->iv_he_bsscolor_change_count);
}

/*
 * Add/Update MBSSID 11ax IE
 */
uint8_t *
ieee80211_add_update_mbssid_ie(u_int8_t *frm, struct ieee80211_node *ni,
                               uint8_t *profiles_cnt, uint8_t subtype)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;
    uint8_t *prof_start, *cap_start, *ie_len_start, *elem_start;
    uint8_t old_ie_len = 0;

    if (!frm) {
        qdf_print("Passed NULL frm to function!! Aborting function..");
        return NULL;
    }

    ie_len_start = frm + 1;

    if (*frm != IEEE80211_ELEMID_MBSSID) {
        /* Add main IE fields */
        *frm = IEEE80211_ELEMID_MBSSID;
        *(frm + 2) = ic->ic_mbss.max_bssid;
        frm += 3;
        old_ie_len = 1;
    } else {
        if (*profiles_cnt == 0) {
            old_ie_len = 1;
            frm += 3;
        } else {
            old_ie_len = *ie_len_start;
            /* Use length field to reach end of IE */
            frm += (*ie_len_start + 2);
        }
    }

    /* Add profile for non-tx VAP */
    *frm = IEEE80211_MBSSID_SUB_ELEMID;
    prof_start = frm;
    frm += 2;

    /* Capability element */
    cap_start = frm;
    *frm++ = IEEE80211_ELEMID_MBSSID_NON_TRANS_CAP;
    *frm++ = 2; /* length */
    ieee80211_add_capability(frm, ni);
    /* Don't set up DMG fields for now */
    frm += 2;

    /* SSID element */
    *frm++ = IEEE80211_ELEMID_SSID;
    if (IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) {
        *frm++ = 0;
    } else {
        *frm++ = ni->ni_esslen; /* length */
        OS_MEMCPY(frm, ni->ni_essid, ni->ni_esslen);
        frm += ni->ni_esslen;
    }

    /* BSSID index element */
    elem_start = frm;
    *frm++ = IEEE80211_ELEMID_MBSSID_INDEX;
    *frm++ = 1; /* length */
    *frm++ = vdev_mlme->mgmt.mbss_11ax.profile_idx;

    if (subtype ==  IEEE80211_FRAME_TYPE_BEACON) {
        *frm++ = vdev_mlme->proto.generic.dtim_period;
        *frm++ = 0;
        *(elem_start + 1) = 3; /* adjust the length */
    }

    /* Add the Application IEs */
    IEEE80211_VAP_LOCK(vap);
    frm = ieee80211_mlme_app_ie_append(vap, IEEE80211_FRAME_TYPE_BEACON, frm);
    IEEE80211_VAP_UNLOCK(vap);

    /* update profile and main element length fields */
    *(prof_start + 1) = frm - cap_start;
    *ie_len_start = frm - prof_start + old_ie_len;
    (*profiles_cnt)++;

    return frm;

} /* end of function */

#ifdef OBSS_PD

/* The offset frame numbers correspond to the octect number of the associated
 * control field in the control frame.
 */
#define IEEE80211_NON_SRG_OBSS_PD_FIELD_OFFSET 4
#define IEEE80211_SRG_OBSS_PD_FIELD_OFFSET     5
#define IEEE80211_SRG_OBSS_PD_TOTAL_FIELD_SIZE \
    (sizeof(ie->srg_obss_pd_min_offset) + \
        sizeof(ie->srg_obss_pd_max_offset) + \
        sizeof(ie->srg_obss_color_bitmap) + \
        sizeof(ie->srg_obss_color_partial_bitmap))


uint8_t *
ieee80211_add_srp_ie(struct ieee80211com *ic, uint8_t *frm)
{
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    struct ieee80211_ie_spatial_reuse *ie = (struct ieee80211_ie_spatial_reuse *) frm;

    OS_MEMSET(ie, 0, sizeof(struct ieee80211_ie_spatial_reuse));

    ie->elem_id = IEEE80211_ELEMID_EXTN;
    ie->elem_len = (sizeof(struct ieee80211_ie_spatial_reuse) -
                                       IEEE80211_IE_HDR_LEN);
    ie->ext_id = IEEE80211_ELEMID_EXT_SRP;

    /* sr_control */
    ie->sr_ctrl.value = cfg_get(psoc, CFG_OL_SRP_SR_CONTROL);

    /* Check SRP_DISALLOWED field in ic and update the associated IE
     * field accordingly.
     */
    ie->sr_ctrl.srp_disallow = ic->ic_he_srctrl_srp_disallowed ?
        !!ic->ic_he_srctrl_srp_disallowed: 0;

    /* Check SR_CTRL_VALUE15_ALLOWED field from ic and update the
     * associated IE field accordingly.
     */
    ie->sr_ctrl.HESIGA_sp_reuse_value15_allowed = ic->ic_he_srctrl_sr15_allowed ?
        !!ic->ic_he_srctrl_sr15_allowed : 0;

    /* Check NON_SRG_OBSS_PD_DISALLOWED and update the value if a user configured
     * value exists, else keep current value.
     */
    ie->sr_ctrl.non_srg_obss_pd_sr_disallowed = ic->ic_he_srctrl_non_srg_obsspd_disallowed ?
        !!ic->ic_he_srctrl_non_srg_obsspd_disallowed : 0;

    /* Check SRG_INFORMATION_PRESENT and update the value if a user configured
     * value exists, else keep current value.
     */
    ie->sr_ctrl.srg_information_present = ic->ic_he_srg_obss_pd_supported ?
        !!ic->ic_he_srg_obss_pd_supported : 0;

    if (ie->sr_ctrl.non_srg_obss_pd_sr_disallowed)  {
        ie->sr_ctrl.non_srg_offset_present = false;
        /* subtract the length of Non-SRG OBSS PD Max Offset */
        ie->elem_len -= 1;
    } else {
        /* If NON_SRG_OBSSPD_DISALLOWED set to false, then non_srg_obsspd
         * based transmissions are allowed. Therefore, set all control fields
         * related to non_srg_obsspd to be present.
         */
        ie->non_srg_obss_pd_max_offset = ic->ic_he_non_srg_obsspd_max_offset;
        ie->sr_ctrl.non_srg_offset_present = true;
    }

    if (ie->sr_ctrl.srg_information_present) {
        ie->srg_obss_pd_min_offset = cfg_get(psoc, CFG_OL_SRP_SRG_OBSS_PD_MIN_OFFSET);
        ie->srg_obss_pd_max_offset = cfg_get(psoc, CFG_OL_SRP_SRG_OBSS_PD_MAX_OFFSET);
        /* set the values of 'SRG BSS Color Bitmap' and 'SRG Partial BSSID Bitmap
         * to all 0s till SRG is feature complete. Currently, only non-SRG based
         * OBSS PD is supported in HK
         */
    } else {
        /* subtract the lengths of the following fields:
         * 1. SRG OBSS PD Min Offset
         * 2. SRG OBSS PD Max Offset
         * 3. SRG BSS Color Bitmap
         * 4. SRG Partial BSSID Bitmap
         */
        ie->elem_len -= (2*8 + 2);
    }

    /* If NON_SRG_OBSSPD control field is disabled but the SRG_OBSSPD control
     * field is set to present, then the frame pointer and ie->elem_len must
     * be updated accordingly to include these fields but not the
     * NON_SRG_OBSS_PD_MAX_OFFSET field. To do this, a memcpy is done to copy
     * the SRG_OBSSPD related fields to compensate for the gap in the IE.
     */
    if(ic->ic_he_srctrl_non_srg_obsspd_disallowed &&
            ic->ic_he_srg_obss_pd_supported) {
        OS_MEMCPY(frm + IEEE80211_NON_SRG_OBSS_PD_FIELD_OFFSET,
        frm + IEEE80211_SRG_OBSS_PD_FIELD_OFFSET,
        IEEE80211_SRG_OBSS_PD_TOTAL_FIELD_SIZE);
    }


    /* advance the frame pointer by 2 octet ie-header + length of ie */
    return frm + ie->elem_len + 2;
}

void
ieee80211_parse_srpie(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct ieee80211_ie_spatial_reuse *srp_ie = (struct ieee80211_ie_spatial_reuse *) ie;
    struct ieee80211_spatial_reuse_handle *ni_srp = &ni->ni_srp;

    ni_srp->obss_min = srp_ie->srg_obss_pd_min_offset;
    ni_srp->obss_max = srp_ie->srg_obss_pd_max_offset;

    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
       "%s SRP IE Params =%x non_srg_obss_pd_max_offset = %d,"
       "srg_obss_pd_min_offset = %d, srg_obss_pd_max_offset = %d\n",
        __func__, srp_ie->sr_ctrl.value,
       srp_ie->non_srg_obss_pd_max_offset,
       srp_ie->srg_obss_pd_min_offset,
       srp_ie->srg_obss_pd_max_offset);

}
#endif

/*
 * routines to parse the IEs received from management frames.
 */
u_int32_t
ieee80211_parse_mpdudensity(u_int32_t mpdudensity)
{
    /*
     * 802.11n D2.0 defined values for "Minimum MPDU Start Spacing":
     *   0 for no restriction
     *   1 for 1/4 us
     *   2 for 1/2 us
     *   3 for 1 us
     *   4 for 2 us
     *   5 for 4 us
     *   6 for 8 us
     *   7 for 16 us
     */
    switch (mpdudensity) {
    case 0:
        return 0;
    case 1:
    case 2:
    case 3:
        /* Our lower layer calculations limit our precision to 1 microsecond */
        return 1;
    case 4:
        return 2;
    case 5:
        return 4;
    case 6:
        return 8;
    case 7:
        return 16;
    default:
        return 0;
    }
}

int
ieee80211_parse_htcap(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct ieee80211_ie_htcap_cmn *htcap = (struct ieee80211_ie_htcap_cmn *)ie;
    struct ieee80211com   *ic = ni->ni_ic;
    struct ieee80211vap   *vap = ni->ni_vap;
    u_int8_t rx_mcs;
    int                    htcapval, prev_htcap = ni->ni_htcap;
    u_int8_t rx_streams = ieee80211_get_rxstreams(ic, vap);
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap);

    if (rx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        rx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    if (tx_streams > IEEE80211_MAX_11N_STREAMS)
    {
        tx_streams = IEEE80211_MAX_11N_STREAMS;
    }

    htcapval    = le16toh(htcap->hc_cap);
    rx_mcs = htcap->hc_mcsset[IEEE80211_TX_MCS_OFFSET];

    rx_mcs &= IEEE80211_TX_MCS_SET;

    if (rx_mcs & IEEE80211_TX_MCS_SET_DEFINED) {
        if( !(rx_mcs & IEEE80211_TX_RX_MCS_SET_NOT_EQUAL) &&
             (rx_mcs & (IEEE80211_TX_MAXIMUM_STREAMS_MASK |IEEE80211_TX_UNEQUAL_MODULATION_MASK))){
            return 0;
        }
    } else {
        if (rx_mcs & IEEE80211_TX_MCS_SET){
            return 0;
        }
    }

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        /*
         * Check if SM powersav state changed.
         * prev_htcap == 0 => htcap set for the first time.
         */
        switch (htcapval & IEEE80211_HTCAP_C_SM_MASK) {
            case IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED:
                if (((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) !=
                     IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED) || !prev_htcap) {
                    /*
                     * Station just disabled SM Power Save therefore we can
                     * send to it at full SM/MIMO.
                     */
                    ni->ni_htcap &= (~IEEE80211_HTCAP_C_SM_MASK);
                    ni->ni_htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED;
                    ni->ni_updaterates = IEEE80211_NODE_SM_EN;
                    IEEE80211_DPRINTF(ni->ni_vap,IEEE80211_MSG_POWER,"%s:SM"
                                      " powersave disabled\n", __func__);
                }
                break;
            case IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC:
                if (((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) !=
                     IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC) || !prev_htcap) {
                    /*
                     * Station just enabled static SM power save therefore
                     * we can only send to it at single-stream rates.
                     */
                    ni->ni_htcap &= (~IEEE80211_HTCAP_C_SM_MASK);
                    ni->ni_htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC;
                    ni->ni_updaterates = IEEE80211_NODE_SM_PWRSAV_STAT;
                    IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_POWER,
                                      "%s:switching to static SM power save\n", __func__);
                }
                break;
            case IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC:
                if (((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) !=
                     IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC) || !prev_htcap) {
                    /*
                     * Station just enabled dynamic SM power save therefore
                     * we should precede each packet we send to it with
                     * an RTS.
                     */
                    ni->ni_htcap &= (~IEEE80211_HTCAP_C_SM_MASK);
                    ni->ni_htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC;
                    ni->ni_updaterates = IEEE80211_NODE_SM_PWRSAV_DYN;
                    IEEE80211_DPRINTF(ni->ni_vap,IEEE80211_MSG_POWER,
                                      "%s:switching to dynamic SM power save\n",__func__);
                }
        }
        IEEE80211_DPRINTF(ni->ni_vap,IEEE80211_MSG_POWER,
                          "%s:calculated updaterates %#x\n",__func__, ni->ni_updaterates);

        ni->ni_htcap = (htcapval & ~IEEE80211_HTCAP_C_SM_MASK) |
            (ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK);

        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_POWER, "%s: ni_htcap %#x\n",
                          __func__, ni->ni_htcap);

        if (htcapval & IEEE80211_HTCAP_C_GREENFIELD)
            ni->ni_htcap |= IEEE80211_HTCAP_C_GREENFIELD;

    } else {
        ni->ni_htcap = htcapval;
    }

        if (ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI40)
            ni->ni_htcap  = ni->ni_htcap & (((vap->iv_htflags & IEEE80211_HTF_SHORTGI40) && vap->iv_sgi)
                                        ? ni->ni_htcap  : ~IEEE80211_HTCAP_C_SHORTGI40);
        if (ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI20)
            ni->ni_htcap  = ni->ni_htcap & (((vap->iv_htflags & IEEE80211_HTF_SHORTGI20) && vap->iv_sgi)
                                        ? ni->ni_htcap  : ~IEEE80211_HTCAP_C_SHORTGI20);

    if (ni->ni_htcap & IEEE80211_HTCAP_C_ADVCODING) {
        if (((vap->vdev_mlme->proto.generic.ldpc & IEEE80211_HTCAP_C_LDPC_TX) == 0) ||
            ((ieee80211com_get_ldpccap(ic) & IEEE80211_HTCAP_C_LDPC_TX) == 0))
            ni->ni_htcap &= ~IEEE80211_HTCAP_C_ADVCODING;
    }

    if (ni->ni_htcap & IEEE80211_HTCAP_C_TXSTBC) {
        ni->ni_htcap  = ni->ni_htcap & (((vap->iv_rx_stbc) && (rx_streams > 1)) ? ni->ni_htcap : ~IEEE80211_HTCAP_C_TXSTBC);
    }

    /* Tx on our side and Rx on the remote side should be considered for STBC with rate control */
    if (ni->ni_htcap & IEEE80211_HTCAP_C_RXSTBC) {
        ni->ni_htcap  = ni->ni_htcap & (((vap->iv_tx_stbc) && (tx_streams > 1)) ? ni->ni_htcap : ~IEEE80211_HTCAP_C_RXSTBC);
    }

    /* Note: when 11ac is enabled the VHTCAP Channel width will override this */
    if (!(ni->ni_htcap & IEEE80211_HTCAP_C_CHWIDTH40)) {
        ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
    } else {
        /* Channel width needs to be set to 40MHz for both 40MHz and 80MHz mode */
        if (ic->ic_cwm_get_width(ic) != IEEE80211_CWM_WIDTH20) {
            ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
        }
    }

    /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on coex rules */
    if ((ni->ni_htcap & IEEE80211_HTCAP_C_INTOLERANT40) &&
        (IEEE80211_IS_CHAN_11N_HT40(vap->iv_bsschan) ||
         IEEE80211_IS_CHAN_11AX_HE40(vap->iv_bsschan))) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_POWER,
                 "%s: Received htcap with 40 intolerant bit set\n", __func__);
        ieee80211node_set_flag(ni, IEEE80211_NODE_40MHZ_INTOLERANT);
    }

    /*
     * The Maximum Rx A-MPDU defined by this field is equal to
     *      (2^^(13 + Maximum Rx A-MPDU Factor)) - 1
     * octets.  Maximum Rx A-MPDU Factor is an integer in the
     * range 0 to 3.
     */

    ni->ni_maxampdu = ((1u << (IEEE80211_HTCAP_MAXRXAMPDU_FACTOR + htcap->hc_maxampdu)) - 1);
    ni->ni_mpdudensity = ieee80211_parse_mpdudensity(htcap->hc_mpdudensity);

    ieee80211node_set_flag(ni, IEEE80211_NODE_HT);

#ifdef ATH_SUPPORT_TxBF

#if ATH_SUPPORT_WAPI
    if(ic->ic_is_mode_offload(ic) || !ieee80211_vap_wapi_is_set(vap)) {
#endif

#if ATH_SUPPORT_WRAP
        /* disable TxBF mode for WRAP in case of Direct Attach only*/
        if(ic->ic_is_mode_offload(ic) || !(vap->iv_wrap || vap->iv_psta)) {
#endif
		ni->ni_mmss = htcap->hc_mpdudensity;
		ni->ni_txbf.value = le32toh(htcap->hc_txbf.value);
		//IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s:get remote txbf ie %x\n",__func__,ni->ni_txbf.value);
		ieee80211_match_txbfcapability(ic, ni);
		//IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s:final result Com ExBF %d, NonCOm ExBF %d, ImBf %d\n",
		//  __func__,ni->ni_explicit_compbf,ni->ni_explicit_noncompbf,ni->ni_implicit_bf );
#if ATH_SUPPORT_WRAP
	}
#endif

#if ATH_SUPPORT_WAPI
    }
#endif

#endif
    if (ic->ic_set_ampduparams) {
        /* Notify LMAC of the ampdu params */
        ic->ic_set_ampduparams(ni);
    }
    return 1;
}

void
ieee80211_parse_htinfo(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct ieee80211_ie_htinfo_cmn  *htinfo = (struct ieee80211_ie_htinfo_cmn *)ie;
    enum ieee80211_cwm_width    chwidth;
    int8_t extoffset;

    switch(htinfo->hi_extchoff) {
    case IEEE80211_HTINFO_EXTOFFSET_ABOVE:
        extoffset = 1;
        break;
    case IEEE80211_HTINFO_EXTOFFSET_BELOW:
        extoffset = -1;
        break;
    case IEEE80211_HTINFO_EXTOFFSET_NA:
    default:
        extoffset = 0;
    }

    chwidth = IEEE80211_CWM_WIDTH20;
    if (extoffset && (htinfo->hi_txchwidth == IEEE80211_HTINFO_TXWIDTH_2040)) {
        chwidth = IEEE80211_CWM_WIDTH40;
    }

    /* update node's recommended tx channel width */
    ni->ni_chwidth = chwidth;

    /* update node's ext channel offset */
    ni->ni_extoffset = extoffset;

    /* update other HT information */
    ni->ni_obssnonhtpresent = htinfo->hi_obssnonhtpresent;
    ni->ni_txburstlimit     = htinfo->hi_txburstlimit;
    ni->ni_nongfpresent     = htinfo->hi_nongfpresent;
}

int
ieee80211_check_mu_client_cap(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct      ieee80211_ie_vhtcap *vhtcap = (struct ieee80211_ie_vhtcap *)ie;
    int         status = 0;

    ni->ni_mu_vht_cap = 0;
    ni->ni_vhtcap = le32toh(vhtcap->vht_cap_info);
    if (ni->ni_vhtcap & IEEE80211_VHTCAP_MU_BFORMEE)
    {
#if WAR_DISABLE_MU_2x2_STA
        /* Disable MU-MIMO for 2x2 Clients */
        if (((ni->ni_vhtcap & IEEE80211_VHTCAP_SOUND_DIM) >> IEEE80211_VHTCAP_SOUND_DIM_S) == 1) {
            return 0;
        }
#endif
        ni->ni_mu_vht_cap = 1;
        status = 1;
    }
    return status;
}

void
ieee80211_parse_vhtcap(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct ieee80211_ie_vhtcap *vhtcap = (struct ieee80211_ie_vhtcap *)ie;
    struct ieee80211com  *ic = ni->ni_ic;
    struct ieee80211vap  *vap = ni->ni_vap;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;

    u_int32_t ampdu_len = 0;
    u_int8_t chwidth = 0;
    u_int8_t rx_streams = ieee80211_get_rxstreams(ic, vap);
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap);
    struct supp_tx_mcs_extnss tx_mcs_extnss_cap;

    /* Negotiated capability set */
    ni->ni_vhtcap = le32toh(vhtcap->vht_cap_info);
    if (ni->ni_vhtcap & IEEE80211_VHTCAP_SHORTGI_80) {
        ni->ni_vhtcap  = ni->ni_vhtcap & ((vap->iv_sgi) ? ni->ni_vhtcap  : ~IEEE80211_VHTCAP_SHORTGI_80);
    }
    if (ni->ni_vhtcap & IEEE80211_VHTCAP_RX_LDPC) {
        ni->ni_vhtcap  = ni->ni_vhtcap & ((vdev_mlme->proto.generic.ldpc & IEEE80211_HTCAP_C_LDPC_TX) ? ni->ni_vhtcap  : ~IEEE80211_VHTCAP_RX_LDPC);
    }
    if (ni->ni_vhtcap & IEEE80211_VHTCAP_TX_STBC) {
        ni->ni_vhtcap  = ni->ni_vhtcap & (((vap->iv_rx_stbc) && (rx_streams > 1)) ? ni->ni_vhtcap : ~IEEE80211_VHTCAP_TX_STBC);
    }

    /* Tx on our side and Rx on the remote side should be considered for STBC with rate control */
    if (ni->ni_vhtcap & IEEE80211_VHTCAP_RX_STBC) {
        ni->ni_vhtcap  = ni->ni_vhtcap & (((vap->iv_tx_stbc) && (tx_streams > 1)) ? ni->ni_vhtcap : ~IEEE80211_VHTCAP_RX_STBC);
    }

    if (ni->ni_vhtcap & IEEE80211_VHTCAP_SU_BFORMEE) {
        ni->ni_vhtcap  = ni->ni_vhtcap & (vdev_mlme->proto.vht_info.subfer ? ni->ni_vhtcap  : ~IEEE80211_VHTCAP_SU_BFORMEE);
    }

    if (ni->ni_vhtcap & IEEE80211_VHTCAP_MU_BFORMEE) {
        ni->ni_vhtcap  = ni->ni_vhtcap & ((vdev_mlme->proto.vht_info.mubfer
                            && vdev_mlme->proto.vht_info.subfer) ? ni->ni_vhtcap  : ~IEEE80211_VHTCAP_MU_BFORMEE);
    }

    if (ic->ic_ext_nss_capable) {
        ni->ni_ext_nss_support  = (ni->ni_vhtcap & IEEE80211_VHTCAP_EXT_NSS_BW_SUPPORT_MASK) >>
                                    IEEE80211_VHTCAP_EXT_NSS_BW_SUPPORT_S;
        if (ni->ni_ext_nss_support)
            ni->ni_prop_ie_used = 0;
    }

    OS_MEMCPY(&tx_mcs_extnss_cap, &vhtcap->tx_mcs_extnss_cap, sizeof(u_int16_t));
    *(u_int16_t *)&tx_mcs_extnss_cap = le16toh(*(u_int16_t*)&tx_mcs_extnss_cap);
    ni->ni_ext_nss_capable  = ((tx_mcs_extnss_cap.ext_nss_capable && ic->ic_ext_nss_capable) ? 1:0);

    if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
        chwidth = vap->iv_chwidth;
    } else {
        chwidth = ic->ic_cwm_get_width(ic);
    }

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        ieee80211_update_ni_chwidth(chwidth, ni, vap);
    }

    if (ni->ni_chwidth == IEEE80211_CWM_WIDTH160) {
        ni->ni_160bw_requested = 1;
    }
    else {
        ni->ni_160bw_requested = 0;
    }

    /*
     * The Maximum Rx A-MPDU defined by this field is equal to
     *   (2^^(13 + Maximum Rx A-MPDU Factor)) - 1
     * octets.  Maximum Rx A-MPDU Factor is an integer in the
     * range 0 to 7.
     */

    ampdu_len = (le32toh(vhtcap->vht_cap_info) & IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP) >> IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP_S;
    ni->ni_maxampdu = (1u << (IEEE80211_VHTCAP_MAX_AMPDU_LEN_FACTOR + ampdu_len)) -1;
    ieee80211node_set_flag(ni, IEEE80211_NODE_VHT);
    ni->ni_tx_vhtrates = le16toh(vhtcap->tx_mcs_map);
    ni->ni_rx_vhtrates = le16toh(vhtcap->rx_mcs_map);
    ni->ni_tx_max_rate = (tx_mcs_extnss_cap.tx_high_data_rate);
    ni->ni_rx_max_rate = le16toh(vhtcap->rx_high_data_rate);

    /* Set NSS for 80+80 MHz same as that for 160 MHz if prop IE is used and AP/STA is capable of 80+80 MHz */
    if ((ni->ni_vhtcap & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160) && ni->ni_prop_ie_used) {
        ni->ni_bw80p80_nss = ni->ni_bw160_nss;
    }
}

void
ieee80211_parse_vhtop(struct ieee80211_node *ni, u_int8_t *ie, u_int8_t *htinfo_ie)
{
    struct ieee80211_ie_vhtop *vhtop = (struct ieee80211_ie_vhtop *)ie;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int ch_width;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    struct ieee80211_ie_htinfo_cmn  *htinfo = (struct ieee80211_ie_htinfo_cmn *)htinfo_ie;
    switch (vhtop->vht_op_chwidth) {
       case IEEE80211_VHTOP_CHWIDTH_2040:
           /* Exact channel width is already taken care of by the HT parse */
       break;
       case IEEE80211_VHTOP_CHWIDTH_80:
       if (ic->ic_ext_nss_capable) {
           if ((extnss_160_validate_and_seg2_indicate(&ni->ni_vhtcap, vhtop, htinfo)) ||
                        (extnss_80p80_validate_and_seg2_indicate(&ni->ni_vhtcap, vhtop, htinfo))) {
               ni->ni_chwidth =  IEEE80211_CWM_WIDTH160;
           } else {
               ni->ni_chwidth =  IEEE80211_CWM_WIDTH80;
           }
       } else if (IS_REVSIG_VHT160(vhtop) || IS_REVSIG_VHT80_80(vhtop)) {
           ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
       } else {
           ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
       }
       break;
       case IEEE80211_VHTOP_CHWIDTH_160:
       case IEEE80211_VHTOP_CHWIDTH_80_80:
           ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
       break;
       default:
           IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_DEBUG,
                            "%s: Unsupported Channel Width\n", __func__);
       break;
    }

    if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
        ch_width = vap->iv_chwidth;
    } else {
        ch_width = ic_cw_width;
    }
    /* Update ch_width only if it is within the user configured width*/
    if(ch_width < ni->ni_chwidth) {
        ni->ni_chwidth = ch_width;
    }
}

void
ieee80211_add_opmode(u_int8_t *frm, struct ieee80211_node *ni,
                    struct ieee80211com *ic,  u_int8_t subtype)
{
    struct ieee80211_ie_op_mode *opmode = (struct ieee80211_ie_op_mode *)frm;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    struct ieee80211vap *vap = ni->ni_vap;
    u_int8_t rx_streams = ieee80211_get_rxstreams(ic, vap);
    u_int8_t nss = vap->vdev_mlme->proto.generic.nss;
    u_int8_t ch_width = 0;
    ieee80211_vht_rate_t ni_tx_vht_rates;


    if ((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ) ||
        (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) {
        /* Check negotiated Rx NSS and chwidth */
        ieee80211_get_vht_rates(ni->ni_tx_vhtrates, &ni_tx_vht_rates);
        rx_streams = MIN(rx_streams, ni_tx_vht_rates.num_streams);
        /* Set negotiated BW */
        ch_width = ni->ni_chwidth;
    } else {
        /* Fill in default channel width */
        if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
            ch_width = vap->iv_chwidth;
        } else {
            ch_width = ic_cw_width;
        }
    }

    opmode->reserved = 0;
    opmode->rx_nss_type = 0; /* No beamforming */
    opmode->rx_nss = nss < rx_streams ? (nss-1) : (rx_streams -1); /* Supported RX streams */
    if (vap->iv_ext_nss_support && (ch_width == IEEE80211_CWM_WIDTH160 || ch_width == IEEE80211_CWM_WIDTH80_80)) {
        opmode->bw_160_80p80 = 1;
    } else {
        opmode->ch_width = ch_width;
    }
}

u_int8_t *
ieee80211_add_addba_ext(u_int8_t *frm, struct ieee80211vap *vap,
                         u_int8_t he_frag)
{
    struct ieee80211_ba_addbaext *addbaextension =
                                 (struct ieee80211_ba_addbaext *)frm;
    u_int8_t addba_ext_len = sizeof(struct ieee80211_ba_addbaext);

    qdf_mem_zero(addbaextension, addba_ext_len);

    addbaextension->elemid      = IEEE80211_ADDBA_EXT_ELEM_ID;
    addbaextension->length      = IEEE80211_ADDBA_EXT_ELEM_ID_LEN;
    addbaextension->no_frag_bit = 0;
    addbaextension->he_fragmentation =
        ((vap->iv_he_frag > he_frag) ? he_frag : vap->iv_he_frag);
    return frm + addba_ext_len;
}

u_int8_t *
ieee80211_add_opmode_notify(u_int8_t *frm, struct ieee80211_node *ni,
                    struct ieee80211com *ic,  u_int8_t subtype)
{
    struct ieee80211_ie_op_mode_ntfy *opmode = (struct ieee80211_ie_op_mode_ntfy *)frm;
    int opmode_notify_len = sizeof(struct ieee80211_ie_op_mode_ntfy);

    opmode->elem_id   = IEEE80211_ELEMID_OP_MODE_NOTIFY;
    opmode->elem_len  =  opmode_notify_len- 2;
    ieee80211_add_opmode((u_int8_t *)&opmode->opmode, ni, ic, subtype);
    return frm + opmode_notify_len;
}

int  ieee80211_intersect_extnss_160_80p80(struct ieee80211_node *ni)
{
    struct ieee80211com  *ic = ni->ni_ic;
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, ni->ni_vap);
    struct ieee80211_bwnss_map nssmap;
    ieee80211_vht_rate_t rx_rrs;

    ieee80211_get_vht_rates(ni->ni_rx_vhtrates, &rx_rrs);

    qdf_mem_zero(&nssmap, sizeof(nssmap));

    if (!ieee80211_get_bw_nss_mapping(ni->ni_vap, &nssmap, tx_streams)) {
        if (!ieee80211_derive_nss_from_cap(ni, &rx_rrs)) {
            return 0;
        }
        ni->ni_bw160_nss = MIN(nssmap.bw_nss_160, ni->ni_bw160_nss);
        ni->ni_bw80p80_nss = MIN(nssmap.bw_nss_160, ni->ni_bw80p80_nss);
    }
    return 1;
}

void
ieee80211_parse_opmode(struct ieee80211_node *ni, u_int8_t *ie, u_int8_t subtype)
{
    struct ieee80211_ie_op_mode *opmode = (struct ieee80211_ie_op_mode *)ie;
    struct ieee80211com  *ic = ni->ni_ic;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, ni->ni_vap);
    u_int8_t rx_nss = 0;
    enum ieee80211_cwm_width ch_width;
#if QCA_SUPPORT_SON
    bool generate_event = false;
#endif
    bool chwidth_change = false;

    /* Check whether this is a beamforming type */
    if (opmode->rx_nss_type == 1) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ACTION,
                           "%s: Beamforming is unsupported\n", __func__);
        return;
    }

    ch_width = get_chwidth_phymode(ni->ni_phymode);

    if(opmode->ch_width > ch_width ||
       (opmode->bw_160_80p80 && (ch_width < IEEE80211_CWM_WIDTH160))) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ACTION,
                          "%s: Unsupported new opmode channel width=%d; "
                          "opmode bw_160_80p80: %d; assoc. ni_chwidth=%d\n",
                          __func__,opmode->ch_width,opmode->bw_160_80p80,
                          ni->ni_chwidth);
        return;
    }

    /* Update ch_width for peer only if it is within the bw supported */
    if ((!ni->ni_ext_nss_support || !opmode->bw_160_80p80)
                   && (opmode->ch_width <= ic_cw_width) &&
                   (opmode->ch_width != ni->ni_chwidth)) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ACTION,
            "%s: Bandwidth changed from %d to %d \n",
             __func__, ni->ni_chwidth, opmode->ch_width);
        switch (opmode->ch_width) {
            case 0:
                ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
                chwidth_change = true;
            break;

            case 1:
                ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
                chwidth_change = true;
            break;

            case 2:
                ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
                chwidth_change = true;
            break;

            case 3:
                if (!ic->ic_ext_nss_capable) {
                    ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
                    chwidth_change = true;
                }
            break;

            default:
                IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ACTION,
                           "%s: Unsupported Channel Width\n", __func__);
                return;
            break;
        }
    }
    if (ni->ni_ext_nss_support && opmode->bw_160_80p80 &&
             (ic_cw_width >= IEEE80211_CWM_WIDTH160) && (ni->ni_chwidth != IEEE80211_CWM_WIDTH160)) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ACTION,
            "%s: Bandwidth changed from %d to 3 \n",
             __func__, ni->ni_chwidth);
        chwidth_change = true;
        ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
    }

    if (chwidth_change == true) {
        if ((subtype != IEEE80211_FC0_SUBTYPE_ASSOC_RESP) &&
            (subtype != IEEE80211_FC0_SUBTYPE_REASSOC_RESP) &&
            (subtype != IEEE80211_FC0_SUBTYPE_REASSOC_REQ) &&
            (subtype != IEEE80211_FC0_SUBTYPE_ASSOC_REQ)) {
            ic->ic_chwidth_change(ni);
#if QCA_SUPPORT_SON
            generate_event = true;
#endif
        }
    }

    /* Propagate the number of Spatial streams to the target */
    rx_nss = opmode->rx_nss + 1;
    if ((rx_nss != ni->ni_streams) && (rx_nss <= tx_streams)) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ACTION,
             "%s: NSS changed from %d to %d \n", __func__, ni->ni_streams, opmode->rx_nss + 1);

        ni->ni_streams = rx_nss;

        if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH160) && ni->ni_ext_nss_support && ni->ni_vap->iv_ext_nss_support){
             ieee80211_intersect_extnss_160_80p80(ni);
        }

        if ((subtype != IEEE80211_FC0_SUBTYPE_ASSOC_RESP) &&
            (subtype != IEEE80211_FC0_SUBTYPE_REASSOC_RESP) &&
            (subtype != IEEE80211_FC0_SUBTYPE_REASSOC_REQ) &&
            (subtype != IEEE80211_FC0_SUBTYPE_ASSOC_REQ)) {
            ic->ic_nss_change(ni);
#if QCA_SUPPORT_SON
            generate_event = true;
#endif
        }
    }

#if QCA_SUPPORT_SON
    if (generate_event) {
        son_send_opmode_update_event(ni->ni_vap->vdev_obj,
                     ni->ni_macaddr,
                     ni->ni_chwidth,
                     ni->ni_streams);
    }
#endif

    /* Updating the node's opmode notify channel width */
    ni->ni_omn_chwidth = ni->ni_chwidth;
}

void
ieee80211_parse_opmode_notify(struct ieee80211_node *ni, u_int8_t *ie, u_int8_t subtype)
{
    struct ieee80211_ie_op_mode_ntfy *opmode = (struct ieee80211_ie_op_mode_ntfy *)ie;
    ieee80211_parse_opmode(ni, (u_int8_t *)&opmode->opmode, subtype);
}

int
ieee80211_parse_dothparams(struct ieee80211vap *vap, u_int8_t *frm)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int len = frm[1];
    u_int8_t chan, tbtt;

    if (len < 4-2) {        /* XXX ie struct definition */
        IEEE80211_DISCARD_IE(vap,
                             IEEE80211_MSG_ELEMID | IEEE80211_MSG_DOTH,
                             "channel switch", "too short, len %u", len);
        return -1;
    }
    chan = frm[3];
    if (isclr(ic->ic_chan_avail, chan)) {
        IEEE80211_DISCARD_IE(vap,
                             IEEE80211_MSG_ELEMID | IEEE80211_MSG_DOTH,
                             "channel switch", "invalid channel %u", chan);
        return -1;
    }
    tbtt = frm[4];
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
                      "%s: channel switch to %d in %d tbtt\n", __func__, chan, tbtt);
    if (tbtt <= 1) {
        struct ieee80211_ath_channel *c;

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DOTH,
                          "%s: Channel switch to %d NOW!\n", __func__, chan);
        if ((c = ieee80211_doth_findchan(vap, chan)) == NULL) {
            /* XXX something wrong */
            IEEE80211_DISCARD_IE(vap,
                                 IEEE80211_MSG_ELEMID | IEEE80211_MSG_DOTH,
                                 "channel switch",
                                 "channel %u lookup failed", chan);
            return 0;
        }
        vap->iv_bsschan = c;
        ieee80211_set_channel(ic, c);
        return 1;
    }
    return 0;
}

int
ieee80211_parse_wmeparams(struct ieee80211vap *vap, u_int8_t *frm,
                          u_int8_t *qosinfo, int forced_update)
{
    struct ieee80211_wme_state *wme = &vap->iv_ic->ic_wme;
    u_int len = frm[1], qosinfo_count;
    int i;

    *qosinfo = 0;

    if (len < sizeof(struct ieee80211_wme_param) - 2) {
        /* XXX: TODO msg+stats */
        return -1;
    }

    *qosinfo = frm[__offsetof(struct ieee80211_wme_param, param_qosInfo)];
    qosinfo_count = *qosinfo & WME_QOSINFO_COUNT;

    if (!forced_update) {

        /* XXX do proper check for wraparound */
        if (qosinfo_count == (wme->wme_wmeChanParams.cap_info & WME_QOSINFO_COUNT))
            return 0;
    }

    frm += __offsetof(struct ieee80211_wme_param, params_acParams);
    for (i = 0; i < WME_NUM_AC; i++) {
        struct wmeParams *wmep =
            &wme->wme_wmeChanParams.cap_wmeParams[i];
        /* NB: ACI not used */
        wmep->wmep_acm = IEEE80211_MS(frm[0], WME_PARAM_ACM);
        wmep->wmep_aifsn = IEEE80211_MS(frm[0], WME_PARAM_AIFSN);
        wmep->wmep_logcwmin = IEEE80211_MS(frm[1], WME_PARAM_LOGCWMIN);
        wmep->wmep_logcwmax = IEEE80211_MS(frm[1], WME_PARAM_LOGCWMAX);
        wmep->wmep_txopLimit = LE_READ_2(frm+2);
        frm += 4;
    }
    wme->wme_wmeChanParams.cap_info = *qosinfo;

    return 1;
}

int
ieee80211_parse_wmeinfo(struct ieee80211vap *vap, u_int8_t *frm,
                        u_int8_t *qosinfo)
{
    struct ieee80211_wme_state *wme = &vap->iv_ic->ic_wme;
    u_int len = frm[1], qosinfo_count;

    *qosinfo = 0;

    if (len < sizeof(struct ieee80211_ie_wme) - 2) {
        /* XXX: TODO msg+stats */
        return -1;
    }

    *qosinfo = frm[__offsetof(struct ieee80211_wme_param, param_qosInfo)];
    qosinfo_count = *qosinfo & WME_QOSINFO_COUNT;

    /* XXX do proper check for wraparound */
    if (qosinfo_count == (wme->wme_wmeChanParams.cap_info & WME_QOSINFO_COUNT))
        return 0;

    wme->wme_wmeChanParams.cap_info = *qosinfo;

    return 1;
}

int
ieee80211_parse_muedcaie(struct ieee80211vap *vap, u_int8_t *frm)
{

    struct ieee80211_ie_muedca *iemuedca = (struct ieee80211_ie_muedca *)frm;
    struct ieee80211_muedca_state *muedca = &vap->iv_muedcastate;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_wme_state *wme = &ic->ic_wme;
    int i;
    u_int8_t qosinfo = frm[3];
    u_int8_t len = frm[1];

    if(len < (sizeof(struct ieee80211_ie_muedca) - 2)) {
        qdf_print("%s: Invalid length for MUEDCA params: %d.", __func__, len);
        return -EINVAL;
    }

    /* This check will set the 'iv_update_muedca_params'(update target)if there
     * is a change in the MUEDCA_UPDATE_COUNT in the qosinfo, irrespective of
     * whether the UPDATE_COUNT was incremented or decremented.
     */
    if(IEEE80211_MS(qosinfo, IEEE80211_MUEDCA_UPDATE_COUNT) !=
                muedca->muedca_param_update_count)
    {
        muedca->muedca_param_update_count =
            IEEE80211_MS(qosinfo, IEEE80211_MUEDCA_UPDATE_COUNT);

        if (vap->iv_he_ul_muofdma)
            vap->iv_update_muedca_params = 1;
    }

    for(i = 0; (i < MUEDCA_NUM_AC) && (vap->iv_update_muedca_params); i++) {

        muedca->muedca_paramList[i].muedca_ecwmin =
            IEEE80211_MS(iemuedca->muedca_param_record[i].ecwminmax,
                    IEEE80211_MUEDCA_ECWMIN);
        muedca->muedca_paramList[i].muedca_ecwmax =
            IEEE80211_MS(iemuedca->muedca_param_record[i].ecwminmax,
                    IEEE80211_MUEDCA_ECWMAX);
        muedca->muedca_paramList[i].muedca_aifsn =
            IEEE80211_MS(iemuedca->muedca_param_record[i].aifsn_aci,
                    IEEE80211_MUEDCA_AIFSN);
        muedca->muedca_paramList[i].muedca_acm =
            IEEE80211_MS(iemuedca->muedca_param_record[i].aifsn_aci,
                    IEEE80211_MUEDCA_ACM);
        muedca->muedca_paramList[i].muedca_timer =
            iemuedca->muedca_param_record[i].timer;

    }

    if(vap->iv_update_muedca_params) {

        if(wme->wme_update) {
            wme->wme_update(ic, vap, true);
        }
        vap->iv_update_muedca_params = 0;
    }

    return 1;

}

int
ieee80211_parse_tspecparams(struct ieee80211vap *vap, u_int8_t *frm)
{
    struct ieee80211_tsinfo_bitmap *tsinfo;

    tsinfo = (struct ieee80211_tsinfo_bitmap *) &((struct ieee80211_wme_tspec *) frm)->ts_tsinfo[0];

    if (tsinfo->tid == 6)
        OS_MEMCPY(&vap->iv_ic->ic_sigtspec, frm, sizeof(struct ieee80211_wme_tspec));
    else
        OS_MEMCPY(&vap->iv_ic->ic_datatspec, frm, sizeof(struct ieee80211_wme_tspec));

    return 1;
}

/*
 * used by STA when it receives a (RE)ASSOC rsp.
 */
int
ieee80211_parse_timeieparams(struct ieee80211vap *vap, u_int8_t *frm)
{
    struct ieee80211_ie_timeout_interval *tieinfo;

    tieinfo = (struct ieee80211_ie_timeout_interval *) frm;

    if (tieinfo->interval_type == IEEE80211_TIE_INTERVAL_TYPE_ASSOC_COMEBACK_TIME)
        vap->iv_assoc_comeback_time = tieinfo->value;
    else
        vap->iv_assoc_comeback_time = 0;

    return 1;
}

/*
 * used by HOST AP when it receives a (RE)ASSOC req.
 */
int
ieee80211_parse_wmeie(u_int8_t *frm, const struct ieee80211_frame *wh,
                      struct ieee80211_node *ni)
{
    u_int len = frm[1];
    u_int8_t ac;

    if (len != 7) {
        IEEE80211_DISCARD_IE(ni->ni_vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WME,
            "WME IE", "too short, len %u", len);
        return -1;
    }
    ni->ni_uapsd = frm[WME_CAPINFO_IE_OFFSET];
    if (ni->ni_uapsd) {
        ieee80211node_set_flag(ni, IEEE80211_NODE_UAPSD);
        switch (WME_UAPSD_MAXSP(ni->ni_uapsd)) {
        case 1:
            ni->ni_uapsd_maxsp = 2;
            break;
        case 2:
            ni->ni_uapsd_maxsp = 4;
            break;
        case 3:
            ni->ni_uapsd_maxsp = 6;
            break;
        default:
            ni->ni_uapsd_maxsp = WME_UAPSD_NODE_MAXQDEPTH;
        }
        for (ac = 0; ac < WME_NUM_AC; ac++) {
            ni->ni_uapsd_ac_trigena[ac] = (WME_UAPSD_AC_ENABLED(ac, ni->ni_uapsd)) ? 1:0;
            ni->ni_uapsd_ac_delivena[ac] = (WME_UAPSD_AC_ENABLED(ac, ni->ni_uapsd)) ? 1:0;
            wlan_peer_update_uapsd_ac_trigena(ni->peer_obj, ni->ni_uapsd_ac_trigena, sizeof(ni->ni_uapsd_ac_trigena));
            wlan_peer_update_uapsd_ac_delivena(ni->peer_obj, ni->ni_uapsd_ac_delivena, sizeof(ni->ni_uapsd_ac_delivena));
        }
        wlan_peer_set_uapsd_maxsp(ni->peer_obj, ni->ni_uapsd_maxsp);
    } else {
        ieee80211node_clear_flag(ni, IEEE80211_NODE_UAPSD);
    }

    IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_POWER, ni,
            "UAPSD bit settings: %02x - vap-%d (%s) from STA %pM\n",
            ni->ni_uapsd, ni->ni_vap->iv_unit, ni->ni_vap->iv_netdev_name,
            ni->ni_macaddr);

    return 1;
}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
/*
 * Parse a WPA information element to collect parameters
 * and validate the parameters against what has been
 * configured for the system.
 */
int
ieee80211_parse_wpa(struct ieee80211vap *vap, u_int8_t *frm,
                    struct ieee80211_rsnparms *rsn)
{
    u_int8_t len = frm[1];
    u_int32_t w;
    int n;

    /*
     * Check the length once for fixed parts: OUI, type,
     * version, mcast cipher, and 2 selector counts.
     * Other, variable-length data, must be checked separately.
     */
    RSN_RESET_AUTHMODE(rsn);
    RSN_SET_AUTHMODE(rsn, IEEE80211_AUTH_WPA);

    if (len < 14) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "WPA", "too short, len %u", len);
        return IEEE80211_REASON_IE_INVALID;
    }
    frm += 6, len -= 4;     /* NB: len is payload only */
    /* NB: iswapoui already validated the OUI and type */
    w = LE_READ_2(frm);
    if (w != WPA_VERSION) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "WPA", "bad version %u", w);
        return IEEE80211_REASON_UNSUPPORTED_RSNE_VER;
    }
    frm += 2, len -= 2;

    /* multicast/group cipher */
    RSN_RESET_MCAST_CIPHERS(rsn);
    w = wpa_cipher(frm, &rsn->rsn_mcastkeylen);
    RSN_SET_MCAST_CIPHER(rsn, w);
    frm += 4, len -= 4;

    /* unicast ciphers */
    n = LE_READ_2(frm);
    frm += 2, len -= 2;
    if (len < n*4+2) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "WPA", "ucast cipher data too short; len %u, n %u",
            len, n);
        return IEEE80211_REASON_INVALID_PAIRWISE_CIPHER;
    }

    RSN_RESET_UCAST_CIPHERS(rsn);
    for (; n > 0; n--) {
        RSN_SET_UCAST_CIPHER(rsn, wpa_cipher(frm, &rsn->rsn_ucastkeylen));
        frm += 4, len -= 4;
    }

    if (rsn->rsn_ucastcipherset == 0) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "WPA", "%s", "ucast cipher set empty");
        return IEEE80211_REASON_INVALID_PAIRWISE_CIPHER;
    }

    /* key management algorithms */
    n = LE_READ_2(frm);
    frm += 2, len -= 2;
    if (len < n*4) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "WPA", "key mgmt alg data too short; len %u, n %u",
            len, n);
        return IEEE80211_REASON_INVALID_AKMP;
    }
    w = 0;
    rsn->rsn_keymgmtset = 0;
    for (; n > 0; n--) {
        w = wpa_keymgmt(frm);
        if (w == WPA_CCKM_AKM) { /* CCKM AKM */
            RSN_SET_AUTHMODE(rsn, IEEE80211_AUTH_CCKM);
            // when AuthMode is CCKM we don't need keymgmtset
            // as AuthMode drives the AKM_CCKM in WPA/RSNIE
            //rsn->rsn_keymgmtset |= 0;
        }
        else
            rsn->rsn_keymgmtset |= (w&0xff);
        frm += 4, len -= 4;
    }

    /* optional capabilities */
    if (len >= 2) {
        rsn->rsn_caps = LE_READ_2(frm);
        frm += 2, len -= 2;
    }

    return 0;
}
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
/*
 * Parse a WPA/RSN information element to collect parameters
 * and validate the parameters against what has been
 * configured for the system.
 */
int
ieee80211_parse_rsn(struct ieee80211vap *vap, u_int8_t *frm,
                    struct ieee80211_rsnparms *rsn)
{
    u_int8_t len = frm[1];
    u_int32_t w;
    int n;

    /*
    * Check the length once for fixed parts:
    * version, mcast cipher, and 2 selector counts.
    * Other, variable-length data, must be checked separately.
    */
    RSN_RESET_AUTHMODE(rsn);
    RSN_SET_AUTHMODE(rsn, IEEE80211_AUTH_RSNA);

    if (len < 2) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "RSN", "too short, len %u", len);
        return IEEE80211_REASON_IE_INVALID;
    }

    frm += 2;
    w = LE_READ_2(frm);
    if (w != RSN_VERSION) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "RSN", "bad version %u", w);
        return IEEE80211_REASON_UNSUPPORTED_RSNE_VER;
    }
    frm += 2, len -= 2;

    if (!len )
    	return IEEE80211_STATUS_SUCCESS;

    if (len < 4) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "RSN", "MCAST too short, len %u", len);
        return IEEE80211_REASON_IE_INVALID;
    }

    /* multicast/group cipher */
    RSN_RESET_MCAST_CIPHERS(rsn);
    w = rsn_cipher(frm, &rsn->rsn_mcastkeylen);
    RSN_SET_MCAST_CIPHER(rsn, w);
    frm += 4, len -= 4;

    if (!len )
    	return IEEE80211_STATUS_SUCCESS;
    if (len < 2) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "RSN", "UCAST too short, len %u", len);
        return IEEE80211_REASON_IE_INVALID;
    }
    /* unicast ciphers */
    n = LE_READ_2(frm);
    frm += 2, len -= 2;
    if (n) {
        if (len < n*4) {
            IEEE80211_DISCARD_IE(vap,
                IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
                "RSN", "ucast cipher data too short; len %u, n %u",
                len, n);
            return IEEE80211_REASON_INVALID_PAIRWISE_CIPHER;
        }
        RSN_RESET_UCAST_CIPHERS(rsn);
        for (; n > 0; n--) {
            RSN_SET_UCAST_CIPHER(rsn, rsn_cipher(frm, &rsn->rsn_ucastkeylen));
            frm += 4, len -= 4;
        }

        if (rsn->rsn_ucastcipherset == 0) {
            IEEE80211_DISCARD_IE(vap,
                IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
                "RSN", "%s", "ucast cipher set empty");
            return IEEE80211_REASON_INVALID_PAIRWISE_CIPHER;
        }
    }


    if (!len )
    	return IEEE80211_STATUS_SUCCESS;

    if (len < 2) {
        IEEE80211_DISCARD_IE(vap,
            IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
            "RSN", "KEY MGMT too short, len %u", len);
        return IEEE80211_REASON_IE_INVALID;
    }

    /* key management algorithms */
    n = LE_READ_2(frm);
    frm += 2, len -= 2;

    if (n) {
        if (len < n*4) {
            IEEE80211_DISCARD_IE(vap,
                  IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
                  "RSN", "key mgmt alg data too short; len %u, n %u",
                  len, n);
            return IEEE80211_REASON_INVALID_AKMP;
        }
        w = 0;
        rsn->rsn_keymgmtset = 0;
        for (; n > 0; n--) {
            w = rsn_keymgmt(frm);
            if (w == RSN_CCKM_AKM) { /* CCKM AKM */
                RSN_SET_AUTHMODE(rsn, IEEE80211_AUTH_CCKM);
                // when AuthMode is CCKM we don't need keymgmtset
                // as AuthMode drives the AKM_CCKM in WPA/RSNIE
                //rsn->rsn_keymgmtset |= 0;
            }
            else {
                rsn->rsn_keymgmtset |= w;
            }
            frm += 4, len -= 4;
        }
    }


    /* optional RSN capabilities */
    if (len >= 2) {
        rsn->rsn_caps = LE_READ_2(frm);
        frm += 2, len -= 2;
        if((rsn->rsn_caps & RSN_CAP_MFP_ENABLED) || (rsn->rsn_caps & RSN_CAP_MFP_REQUIRED)){
            RSN_RESET_MCASTMGMT_CIPHERS(rsn);
            w = IEEE80211_CIPHER_AES_CMAC;
            RSN_SET_MCASTMGMT_CIPHER(rsn, w);
        }
    }

    /* optional XXXPMKID */
    if (len >= 2) {
        n = LE_READ_2(frm);
        frm += 2, len -= 2;
        /* Skip them */
        if (len < n*16) {
            IEEE80211_DISCARD_IE(vap,
                IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
                "RSN", "key mgmt OMKID data too short; len %u, n %u",
                len, n);
            return IEEE80211_REASON_INVALID_AKMP;
        }
        frm += n * 16, len -= n * 16;
    }

    /* optional multicast/group management frame cipher */
    if (len >= 4) {
        RSN_RESET_MCASTMGMT_CIPHERS(rsn);
        w = rsn_cipher(frm, NULL);
        if((w != IEEE80211_CIPHER_AES_CMAC)
           && (w != IEEE80211_CIPHER_AES_CMAC_256)
           && (w != IEEE80211_CIPHER_AES_GMAC)
           && (w != IEEE80211_CIPHER_AES_GMAC_256) ){
            IEEE80211_DISCARD_IE(vap,
                IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
                "RSN", "invalid multicast/group management frame cipher; len %u, n %u",
                len, n);
            return IEEE80211_REASON_IE_INVALID;
        }
        RSN_SET_MCASTMGMT_CIPHER(rsn, w);
        frm += 4, len -= 4;
    }

    return IEEE80211_STATUS_SUCCESS;
}
#endif
void
ieee80211_savenie(osdev_t osdev,u_int8_t **iep, const u_int8_t *ie, u_int ielen)
{
    /*
    * Record information element for later use.
    */
    if (*iep == NULL || (*iep)[1] != ie[1])
    {
        if (*iep != NULL)
            OS_FREE(*iep);
		*iep = (void *) OS_MALLOC(osdev, ielen, GFP_KERNEL);
    }
    if (*iep != NULL)
        OS_MEMCPY(*iep, ie, ielen);
}

void
ieee80211_saveie(osdev_t osdev,u_int8_t **iep, const u_int8_t *ie)
{
    u_int ielen = ie[1]+2;
    ieee80211_savenie(osdev,iep, ie, ielen);
}

void
ieee80211_process_athextcap_ie(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct ieee80211_ie_ath_extcap *athextcapIe =
        (struct ieee80211_ie_ath_extcap *) ie;
    u_int16_t remote_extcap = athextcapIe->ath_extcap_extcap;

    remote_extcap = LE_READ_2(&remote_extcap);

    /* We know remote node is an Atheros Owl or follow-on device */
    ieee80211node_set_flag(ni, IEEE80211_NODE_ATH);

    /* If either one of us is capable of OWL WDS workaround,
     * implement WDS mode block-ack corruption workaround
     */
    if (remote_extcap & IEEE80211_ATHEC_OWLWDSWAR) {
        ieee80211node_set_flag(ni, IEEE80211_NODE_OWL_WDSWAR);
    }

    /* If device and remote node support the Atheros proprietary
     * wep/tkip aggregation mode, mark node as supporting
     * wep/tkip w/aggregation.
     * Save off the number of rx delimiters required by the destination to
     * properly receive tkip/wep with aggregation.
     */
    if (remote_extcap & IEEE80211_ATHEC_WEPTKIPAGGR) {
        ieee80211node_set_flag(ni, IEEE80211_NODE_WEPTKIPAGGR);
        ni->ni_weptkipaggr_rxdelim = athextcapIe->ath_extcap_weptkipaggr_rxdelim;
    }
    /* Check if remote device, require extra delimiters to be added while
     * sending aggregates. Osprey 1.0 and earlier chips need this.
     */
    if (remote_extcap & IEEE80211_ATHEC_EXTRADELIMWAR) {
        ieee80211node_set_flag(ni, IEEE80211_NODE_EXTRADELIMWAR);
    }

}

void
ieee80211_parse_athParams(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_ie_athAdvCap *athIe =
            (struct ieee80211_ie_athAdvCap *) ie;

    (void)vap;
    (void)ic;

    ni->ni_ath_flags = athIe->athAdvCap_capability;
    if (ni->ni_ath_flags & IEEE80211_ATHC_COMP)
        ni->ni_ath_defkeyindex = LE_READ_2(&athIe->athAdvCap_defKeyIndex);
}

/*support for WAPI: parse WAPI IE*/
#if ATH_SUPPORT_WAPI
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
/*
 * Parse a WAPI information element to collect parameters
 * and validate the parameters against what has been
 * configured for the system.
 */
int
ieee80211_parse_wapi(struct ieee80211vap *vap, u_int8_t *frm,
	struct ieee80211_rsnparms *rsn)
{
	u_int8_t len = frm[1];
	u_int32_t w;
	int n;

	/*
	 * Check the length once for fixed parts:
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
    RSN_RESET_AUTHMODE(rsn);
    RSN_SET_AUTHMODE(rsn, IEEE80211_AUTH_WAPI);

	if (!ieee80211_vap_wapi_is_set(vap)) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "vap not WAPI, flags 0x%x", vap->iv_flags);
		return IEEE80211_REASON_IE_INVALID;
	}

	if (len < 20) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "too short, len %u", len);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 2;
	w = LE_READ_2(frm);
	if (w != WAPI_VERSION) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "bad version %u", w);
		return IEEE80211_REASON_UNSUPPORTED_RSNE_VER;
	}
	frm += 2, len -= 2;

	/* key management algorithms */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "key mgmt alg data too short; len %u, n %u",
		    len, n);
		return IEEE80211_REASON_INVALID_AKMP;
	}
	w = 0;
	for (; n > 0; n--) {
		w |= wapi_keymgmt(frm);
		frm += 4, len -= 4;
	}
	if (w == 0) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "%s", "no acceptable key mgmt alg");
		return IEEE80211_REASON_INVALID_AKMP;

	}
    rsn->rsn_keymgmtset = w & WAPI_ASE_WAI_AUTO;

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4+2) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "ucast cipher data too short; len %u, n %u",
		    len, n);
		return IEEE80211_REASON_INVALID_PAIRWISE_CIPHER;
	}
	w = 0;
    RSN_RESET_UCAST_CIPHERS(rsn);
	for (; n > 0; n--) {
		w |= 1<<wapi_cipher(frm, &rsn->rsn_ucastkeylen);
		frm += 4, len -= 4;
	}
	if (w == 0) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "%s", "ucast cipher set empty");
		return IEEE80211_REASON_INVALID_PAIRWISE_CIPHER;
	}
	if (w & (1<<IEEE80211_CIPHER_WAPI)){
		RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_WAPI);}
	else{
		RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_WAPI);}

	/* multicast/group cipher */
    RSN_RESET_MCAST_CIPHERS(rsn);
	w = wapi_cipher(frm, &rsn->rsn_mcastkeylen);
    RSN_SET_MCAST_CIPHER(rsn, w);
	if (!RSN_HAS_MCAST_CIPHER(rsn, IEEE80211_CIPHER_WAPI)) {
		IEEE80211_DISCARD_IE(vap,
		    IEEE80211_MSG_ELEMID | IEEE80211_MSG_WPA,
		    "WAPI", "mcast cipher mismatch; got %u, expected %u",
		    w, IEEE80211_CIPHER_WAPI);
		return IEEE80211_REASON_IE_INVALID;
	}
	frm += 4, len -= 4;


	/* optional RSN capabilities */
	if (len > 2)
		rsn->rsn_caps = LE_READ_2(frm);
	/* XXXPMKID */

	return 0;
}
#endif
#endif /*ATH_SUPPORT_WAPI*/

static int
ieee80211_query_ie(struct ieee80211vap *vap, u_int8_t *frm)
{
    switch (*frm) {
        case IEEE80211_ELEMID_RSN:
            if ((frm[1] + 2) < IEEE80211_RSN_IE_LEN) {
                return -EOVERFLOW;
            }
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
            wlan_crypto_build_rsnie(vap->vdev_obj, frm, NULL);
#else
            ieee80211_setup_rsn_ie(vap, frm);
#endif
            break;
        default:
            break;
    }
    return EOK;
}

int
wlan_get_ie(wlan_if_t vaphandle, u_int8_t *frm)
{
    return ieee80211_query_ie(vaphandle, frm);
}

uint32_t ieee80211_get_max_chan_switch_time(struct ieee80211_max_chan_switch_time_ie *mcst_ie)
{
    uint32_t  max_chan_switch_time = 0;
    uint8_t i;

    /* unpack the max_time in 3 octets/bytes. Little endian format */
    for(i = 0; i < SIZE_OF_MAX_TIME_INT; i++) {
        max_chan_switch_time  += (mcst_ie->switch_time[i] << (i * BITS_IN_A_BYTE));
    }
    return max_chan_switch_time;
}

struct ieee80211_ath_channel *
ieee80211_get_new_sw_chan (
    struct ieee80211_node                       *ni,
    struct ieee80211_ath_channelswitch_ie           *chanie,
    struct ieee80211_extendedchannelswitch_ie   *echanie,
    struct ieee80211_ie_sec_chan_offset         *secchanoffsetie,
    struct ieee80211_ie_wide_bw_switch         *widebwie,
    u_int8_t *cswarp
    )
{
    struct ieee80211_ath_channel *chan;
    struct ieee80211com *ic = ni->ni_ic;
    enum ieee80211_phymode phymode = IEEE80211_MODE_AUTO;
    u_int8_t    secchanoffset = 0;
    u_int8_t    newchannel = 0;
    u_int8_t    newcfreq2 = 0;
    enum ieee80211_cwm_width secoff_chwidth = IEEE80211_CWM_WIDTH20;
    enum ieee80211_cwm_width wideband_chwidth = IEEE80211_CWM_WIDTH20;
    enum ieee80211_cwm_width dest_chwidth = IEEE80211_CWM_WIDTH20;
    enum ieee80211_cwm_width max_allowed_chwidth = IEEE80211_CWM_WIDTH20;
    enum ieee80211_mode mode = IEEE80211_MODE_INVALID;

   if(chanie) {
        newchannel = chanie->newchannel;
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SCANENTRY,
            "%s: CSA new channel = %d\n",
             __func__, chanie->newchannel);
    } else if (echanie) {
        newchannel = echanie->newchannel;
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SCANENTRY,
            "%s: E-CSA new channel = %d\n",
             __func__, echanie->newchannel);
    }


   if(widebwie) {
        newcfreq2 = widebwie->new_ch_freq_seg2;
        wideband_chwidth = widebwie->new_ch_width;
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SCANENTRY,
            "%s: wide bandwidth changed new vht chwidth = %d"
            " cfreq1: %d, cfreq2: %d\n", __func__, widebwie->new_ch_width,
            widebwie->new_ch_freq_seg1, newcfreq2);
    }

    if(secchanoffsetie) {
        secchanoffset = secchanoffsetie->sec_chan_offset;
        if ((secchanoffset == IEEE80211_SEC_CHAN_OFFSET_SCA) ||
            (secchanoffset == IEEE80211_SEC_CHAN_OFFSET_SCB)) {
            secoff_chwidth = IEEE80211_CWM_WIDTH40;
        } else {
            secchanoffset = IEEE80211_SEC_CHAN_OFFSET_SCN;
            secoff_chwidth = IEEE80211_CWM_WIDTH20;
        }
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_SCANENTRY,
                "%s: HT bandwidth changed new secondary channel offset = %d\n",
                __func__, secchanoffset);
    }

    /* Channel switch announcement can't be used to switch
     * between 11a/b/g/n/ac modes. Only channel and width can
     * be switched. So first find the current association
     * mode and then find channel width announced by AP and
     * mask it with maximum channel width allowed by user
     * configuration.
     * For finding a/b/g/n/ac.. modes, current operating
     * channel mode can be used.
     */

    /* Below algorithm is used to derive the best channel:
     * 1. Find current operating mode a/b/g/n/ac from current channel --> M.
     * 2. Find new channel width announced by AP --> W.
     * 3. Mask W with max user configured width and find desired chwidth--> X.
     * 4. Combine M and X to find new composite phymode --> C.
     * 5. Check composite phymode C is supported by device.
     * 6. If step 5 evaluates true, C is the destination composite phymode.
     * 7. If step 5 evaluates false, find a new composite phymode C from M and X/2.
     * 8. Repeat until a device supported phymode is found or all channel
          width combinations are exausted.
     * 9. Find the new channel with C, cfreq1 and cfreq2.
     */

    /* Find current mode */
    mode = ieee80211_get_mode(ic);
    if (mode == IEEE80211_MODE_INVALID) {
        /* No valid mode found. */
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD,
            IEEE80211_MSG_SCANENTRY, "%s : Invalid current mode\n", __func__);
        return NULL;
    }

    /* Calculate destination channel width */
    if (widebwie) {
        switch (wideband_chwidth) {
            case IEEE80211_VHTOP_CHWIDTH_2040:
                {
                    /* Wide band IE is never present for 20MHz */
                    dest_chwidth = IEEE80211_CWM_WIDTH40;
                    break;
                }
            case IEEE80211_VHTOP_CHWIDTH_80:
                {
                    dest_chwidth = IEEE80211_CWM_WIDTH80;
                    break;
                }
            case IEEE80211_VHTOP_CHWIDTH_160:
                {
                    dest_chwidth = IEEE80211_CWM_WIDTH160;
                    break;
                }
            case IEEE80211_VHTOP_CHWIDTH_80_80:
                {
                    dest_chwidth = IEEE80211_CWM_WIDTH80_80;
                    break;
                }
            default:
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_SCANENTRY,
                 "%s : Invalid wideband channel width %d specified\n",
                  __func__, wideband_chwidth);
                return NULL;
        }
    } else if (secchanoffsetie) {
        dest_chwidth = secoff_chwidth;
    }

    /* Find maximum allowed chwidth */
    max_allowed_chwidth = ieee80211_get_vap_max_chwidth(ni->ni_vap);
    if ((dest_chwidth == IEEE80211_CWM_WIDTH80_80) && (max_allowed_chwidth == IEEE80211_CWM_WIDTH160)) {
        dest_chwidth = IEEE80211_CWM_WIDTH80;
    } else if ((dest_chwidth == IEEE80211_CWM_WIDTH160) && (max_allowed_chwidth == IEEE80211_CWM_WIDTH80_80)) {
        dest_chwidth = IEEE80211_CWM_WIDTH160;
    } else if (dest_chwidth > max_allowed_chwidth) {
        dest_chwidth = max_allowed_chwidth;
    }

    /* 11N and 11AX modes are supported on both 5G and 2.4G bands.
     * Find which band AP is going to.
     */
     if (mode == IEEE80211_MODE_N) {
         if (newchannel > 20) {
             mode = IEEE80211_MODE_NA;
         } else {
             mode = IEEE80211_MODE_NG;
         }
     } else if (mode == IEEE80211_MODE_AX) {
         if (newchannel > 20) {
             mode = IEEE80211_MODE_AXA;
         } else {
             mode = IEEE80211_MODE_AXG;
         }
     }

    do {
        /* Calculate destination composite Phymode and ensure device support */
        phymode = ieee80211_get_composite_phymode(mode, dest_chwidth, secchanoffset);
        if (phymode == IEEE80211_MODE_AUTO) {
            /* Could not find valid destination Phymode. */
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_SCANENTRY,
            "%s : Could not find valid destination Phymode for mode: %d "
            "dest_chwidth: %d, secchanoffset: %d\n",__func__, mode, dest_chwidth,
            secchanoffset);
            return NULL;
        }
        if (IEEE80211_SUPPORT_PHY_MODE(ic, phymode)) {
            break;
        } else {
            /* If composite phymode is not supported by device,
             * try finding next composite phymode having lower chwidth
             */
            if ((dest_chwidth == IEEE80211_CWM_WIDTH160) ||
                (dest_chwidth == IEEE80211_CWM_WIDTH80_80)) {
                dest_chwidth = IEEE80211_CWM_WIDTH80;
            } else {
                dest_chwidth -= 1;
            }
        }
    } while (dest_chwidth >= IEEE80211_CWM_WIDTH20);

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_SCANENTRY,
    "%s : New composite phymode is: %d", __func__, phymode);

    chan  =  ieee80211_find_dot11_channel(ic, newchannel, newcfreq2, phymode);
    if (!chan) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_SCANENTRY,
                "%s : Couldn't find new channel with phymode: %d, "
                "newchannel: %d, newcfreq2: %d\n", __func__, phymode,
                newchannel, newcfreq2);
        return chan;
    }

    return chan;
}

/*update the bss channel info on all vaps */
static void ieee80211_vap_iter_update_bss_chan(void *arg, struct ieee80211vap *vap)
{
      struct ieee80211_ath_channel *bsschan = (struct ieee80211_ath_channel *) arg;
      vap->iv_bsschan = bsschan;
      vap->iv_des_mode = wlan_get_bss_phymode(vap);
      vap->iv_des_chan[vap->iv_des_mode] = bsschan;

      if(vap->iv_bss)
      {
          vap->iv_bss->ni_chan = bsschan;
      }
}

#if ATH_SUPPORT_IBSS_DFS
/*
* initialize a IBSS dfs ie
*/
void
ieee80211_build_ibss_dfs_ie(struct ieee80211vap *vap)
{
    u_int ch_count;
    struct ieee80211com *ic = vap->iv_ic;

    if (vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt == 0) {
        vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt = INIT_IBSS_DFS_OWNER_RECOVERY_TIME_IN_TBTT;
        vap->iv_ibss_dfs_csa_measrep_limit = DEFAULT_MAX_CSA_MEASREP_ACTION_PER_TBTT;
        vap->iv_ibss_dfs_csa_threshold     = ic->ic_chan_switch_cnt;
    }
    OS_MEMZERO(vap->iv_ibssdfs_ie_data.ch_map_list, sizeof(struct channel_map_field) * (IEEE80211_CHAN_MAX + 1));
    vap->iv_ibssdfs_ie_data.ie = IEEE80211_ELEMID_IBSSDFS;
    vap->iv_ibssdfs_ie_data.rec_interval = vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt;
    ch_count = ieee80211_create_dfs_channel_list(ic, vap->iv_ibssdfs_ie_data.ch_map_list);
    vap->iv_ibssdfs_ie_data.len = IBSS_DFS_ZERO_MAP_SIZE + (sizeof(struct channel_map_field) * ch_count);
    vap->iv_measrep_action_count_per_tbtt = 0;
    vap->iv_csa_action_count_per_tbtt = 0;
    vap->iv_ibssdfs_recovery_count = vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt;
}
#endif
/*
 * is_vap_start_success(): Return the status of vap's start response.
 * @vap: Pointer to vap structure.
 */
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
static bool is_vap_start_success(struct ieee80211vap *vap)
{
    return vap->vap_start_failure;
}
#else
static bool is_vap_start_success(struct ieee80211vap *vap)
{
    return true;
}
#endif
/*
 * Execute radar channel change. This is called when a radar/dfs
 * signal is detected.  AP mode only. chan_failure indicates, whether
 * this API is invoked on restart resp failure. Return 1 on success, 0 on
 * failure.
 */
int
ieee80211_dfs_action(struct ieee80211vap *vap, struct
                     ieee80211_ath_channelswitch_ie *pcsaie,
                     bool chan_failure)
{
    struct ieee80211com *ic = vap->iv_ic;
    uint8_t target_chan_ieee = 0;
    struct ieee80211_ath_channel *ptarget_channel = NULL;
    struct ieee80211vap *tmp_vap = NULL;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct ch_params ch_params;
    enum ieee80211_phymode chan_mode;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops = NULL;
    bool bw_reduce = false;

    if (vap->iv_opmode != IEEE80211_M_HOSTAP &&
        vap->iv_opmode != IEEE80211_M_IBSS) {
        return 0;
    }

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is null", __func__);
        return -1;
    }

    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL) {
	QDF_TRACE(QDF_MODULE_ID_DFS, QDF_TRACE_LEVEL_DEBUG, "psoc is null");
        return -1;
    }


    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
    ieee80211_regdmn_get_des_chan_params(vap, &ch_params);

#if ATH_SUPPORT_IBSS_DFS
    if (vap->iv_opmode == IEEE80211_M_IBSS) {

        if (pcsaie) {
            ptarget_channel = ieee80211_doth_findchan(vap, pcsaie->newchannel);
        } else if(ic->ic_flags & IEEE80211_F_CHANSWITCH) {
            ptarget_channel = ieee80211_doth_findchan(vap, ic->ic_chanchange_chan);
        } else {
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                    QDF_STATUS_SUCCESS) {
                return -1;
            }
            target_chan_ieee = mlme_dfs_random_channel(pdev, &ch_params, 0);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);

            if (target_chan_ieee) {
                chan_mode = ieee80211_get_target_channel_mode(ic, &ch_params);
                ptarget_channel = ieee80211_find_dot11_channel(ic,
                        target_chan_ieee, ch_params.center_freq_seg1,
                        chan_mode);
            } else {
                ptarget_channel = NULL;
                ic->no_chans_available = 1;
                qdf_print("%s: vap-%d(%s) channel is not available, bringdown all the AP vaps",
                        __func__,vap->iv_unit,vap->iv_netdev_name);
                osif_bring_down_vaps(ic, vap);
            }
        }
    }
#endif /* ATH_SUPPORT_IBSS_DFS */

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        if(IEEE80211_IS_CSH_OPT_APRIORI_NEXT_CHANNEL_ENABLED(ic) && ic->ic_tx_next_ch)
        {
            ptarget_channel = ic->ic_tx_next_ch;
        } else {
            /*
             *  1) If nonDFS random is requested then first try selecting a nonDFS
             *     channel, if not found try selecting a DFS channel.
             *  2) By default the random selection from both DFS and nonDFS set.
             */

            if (IEEE80211_IS_CSH_NONDFS_RANDOM_ENABLED(ic)) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                target_chan_ieee = mlme_dfs_random_channel(pdev, &ch_params,
                        DFS_RANDOM_CH_FLAG_NO_DFS_CH);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            if (!target_chan_ieee) {
                if (dfs_rx_ops && dfs_rx_ops->dfs_is_bw_reduction_needed) {
                    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                            QDF_STATUS_SUCCESS) {
                        return -1;
                    }

                dfs_rx_ops->dfs_is_bw_reduction_needed(pdev, &bw_reduce);

                if(bw_reduce)
                    target_chan_ieee = mlme_dfs_bw_reduced_channel(pdev,
                                                                   &ch_params);
                }

                if(!target_chan_ieee)
                    target_chan_ieee = mlme_dfs_random_channel(pdev,
                                                               &ch_params, 0);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            if (target_chan_ieee) {
                chan_mode = ieee80211_get_target_channel_mode(ic, &ch_params);
                ptarget_channel = ieee80211_find_dot11_channel(ic,
                        target_chan_ieee, ch_params.center_freq_seg1,
                        chan_mode);
            } else {
                ptarget_channel = NULL;
                ic->no_chans_available = 1;
                qdf_print("%s: vap-%d(%s) channel is not available, bringdown all the AP vaps",
                        __func__,vap->iv_unit,vap->iv_netdev_name);
                osif_bring_down_vaps(ic, vap);
            }
        }
    }

    /* If we do have a scan entry, make sure its not an excluded 11D channel.
       See bug 31246 */
    /* No channel was found via scan module, means no good scanlist
       was found */

    if (ptarget_channel)
    {
        if (IEEE80211_IS_CHAN_11AXA_HE160(ptarget_channel)) {
            qdf_print("Changing to HE160 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        } else if (IEEE80211_IS_CHAN_11AXA_HE80_80(ptarget_channel)) {
            qdf_print("Changing to HE80_80 Primary %s channel %d (%d MHz) secondary %s chan %d (center freq %d)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq,
                    IEEE80211_IS_CHAN_DFS_CFREQ2(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_vhtop_ch_freq_seg2,
                    ieee80211_ieee2mhz(ic, ptarget_channel->ic_vhtop_ch_freq_seg2, ptarget_channel->ic_flags));
        } else if (IEEE80211_IS_CHAN_11AXA_HE80(ptarget_channel)) {
            qdf_print("Changing to HE80 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        } else if (IEEE80211_IS_CHAN_11AXA_HE40(ptarget_channel)) {
            qdf_print("Changing to HE40 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        } else if (IEEE80211_IS_CHAN_11AXA_HE20(ptarget_channel)) {
            qdf_print("Changing to HE20 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        } else if (IEEE80211_IS_CHAN_11AC_VHT160 (ptarget_channel)) {
            qdf_print("Changing to HT160 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        } else if (IEEE80211_IS_CHAN_11AC_VHT80_80(ptarget_channel)) {
            qdf_print("Changing to HT80_80 Primary %s channel %d (%d MHz) secondary %s chan %d (center freq %d)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq,
                    IEEE80211_IS_CHAN_DFS_CFREQ2(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_vhtop_ch_freq_seg2,
                    ieee80211_ieee2mhz(ic, ptarget_channel->ic_vhtop_ch_freq_seg2, ptarget_channel->ic_flags));
        } else if (IEEE80211_IS_CHAN_11AC_VHT80(ptarget_channel)) {
            qdf_print("Changing to HT80 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        } else if (IEEE80211_IS_CHAN_11AC_VHT40(ptarget_channel)) {
            qdf_print("Changing to HT40 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        } else if (IEEE80211_IS_CHAN_11AC_VHT20(ptarget_channel)) {
            qdf_print("Changing to HT20 %s channel %d (%d MHz)",
                    IEEE80211_IS_CHAN_DFS(ptarget_channel) ? "DFS" : "non-DFS",
                    ptarget_channel->ic_ieee,
                    ptarget_channel->ic_freq);
        }
        /* In case of NOL failure in vap's start response(vap start req given
         * on NOL channel), though the vap is in run state, do not initiate
         * vdev restart on a new channel using CSA in beacon update as
         * transmission on NOL channel is a violation.
         */

        if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) &&
            !(is_vap_start_success(vap)))
        {
            if (pcsaie) {
                ic->ic_chanchange_chan = pcsaie->newchannel;
                ic->ic_chanchange_tbtt = pcsaie->tbttcount;
            } else {
                ic->ic_chanchange_channel = ptarget_channel;
                ic->ic_chanchange_secoffset =
                    ieee80211_sec_chan_offset(ic->ic_chanchange_channel);
                ic->ic_chanchange_chwidth =
                    ieee80211_get_chan_width(ic->ic_chanchange_channel);
                ic->ic_chanchange_chan = ptarget_channel->ic_ieee;
                ic->ic_chanchange_tbtt = ic->ic_chan_switch_cnt;
            }

            if (IEEE80211_IS_CHAN_11AC_VHT160(ptarget_channel)) {
                vap->iv_des_mode   = IEEE80211_MODE_11AC_VHT160;
            }
            if (IEEE80211_IS_CHAN_11AC_VHT80_80(ptarget_channel)) {
                vap->iv_des_cfreq2 =  ptarget_channel->ic_vhtop_ch_freq_seg2;
                vap->iv_des_mode   = IEEE80211_MODE_11AC_VHT80_80;
            }
            if (IEEE80211_IS_CHAN_11AC_VHT80(ptarget_channel)) {
                vap->iv_des_mode   = IEEE80211_MODE_11AC_VHT80;
            }

#ifdef MAGPIE_HIF_GMAC
            TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
                ic->ic_chanchange_cnt += ic->ic_chanchange_tbtt;
            }
#endif
            if (ic->ic_flags & IEEE80211_F_CHANSWITCH) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_DFS, "Channel change is already on");
                return -1;
            }

            ic->ic_flags |= IEEE80211_F_CHANSWITCH;

            wlan_pdev_mlme_vdev_sm_notify_radar_ind(pdev, ptarget_channel);

#if ATH_SUPPORT_IBSS_DFS
            if (vap->iv_opmode == IEEE80211_M_IBSS) {
                struct ieee80211_action_mgt_args *actionargs;
                /* overwirte with our own value if we are DFS owner */
                if ( pcsaie == NULL &&
                        vap->iv_ibssdfs_state == IEEE80211_IBSSDFS_OWNER) {
                    ic->ic_chanchange_tbtt = vap->iv_ibss_dfs_csa_threshold;
                }

                vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_CHANNEL_SWITCH;

                if(vap->iv_csa_action_count_per_tbtt > vap->iv_ibss_dfs_csa_measrep_limit) {
                    return 1;
                }
                vap->iv_csa_action_count_per_tbtt++;

                ieee80211_ic_doth_set(ic);

                vap->iv_channelswitch_ie_data.newchannel = ic->ic_chanchange_chan;

                /* use beacon_update function to do real channel switch */
                ieee80211_ibss_beacon_update_start(ic);

                if(IEEE80211_ADDR_EQ(vap->iv_ibssdfs_ie_data.owner, vap->iv_myaddr))
                {
                    actionargs = OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(struct ieee80211_action_mgt_args) , GFP_KERNEL);
                    if (actionargs == NULL) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Unable to alloc arg buf. Size=%d\n",
                                __func__, sizeof(struct ieee80211_action_mgt_args));
                        return 0;
                    }
                    OS_MEMZERO(actionargs, sizeof(struct ieee80211_action_mgt_args));

                    actionargs->category = IEEE80211_ACTION_CAT_SPECTRUM;
                    actionargs->action   = IEEE80211_ACTION_CHAN_SWITCH;
                    actionargs->arg1     = 1;   /* mode? no use for now */
                    actionargs->arg2     = ic->ic_chanchange_chan;
                    ieee80211_send_action(vap->iv_bss, actionargs, NULL);
                    OS_FREE(actionargs);
                }
            }
#endif /* ATH_SUPPORT_IBSS_DFS */
        }
        else
        {
            /*
             * vap is not in run  state yet. so
             * change the channel here.
             */
            ic->ic_chanchange_chan = ptarget_channel->ic_ieee;

            /* update the bss channel of all the vaps */
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_update_bss_chan, ptarget_channel);
            ic->ic_prevchan = ic->ic_curchan;
            TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
                if (tmp_vap->iv_opmode == IEEE80211_M_MONITOR ||
                        (tmp_vap->iv_opmode == IEEE80211_M_HOSTAP)) {
                    if ((wlan_vdev_chan_config_valid(tmp_vap->vdev_obj) !=
                                                       QDF_STATUS_SUCCESS) &&
                        (qdf_atomic_read(&(tmp_vap->iv_is_start_sent)))) {
                        qdf_atomic_set(&(tmp_vap->iv_restart_pending), 1);
                    }
                }
            }
            if (chan_failure)
                 wlan_pdev_mlme_vdev_sm_notify_chan_failure(pdev, ptarget_channel);
            else
                 wlan_pdev_mlme_vdev_sm_notify_radar_ind(pdev, ptarget_channel);
        }
    }
    else
    {
        /* should never come here? */
        qdf_print("Cannot change to any channel");
        return 0;
    }
    return 1;
}

void ieee80211_bringup_ap_vaps(struct ieee80211com *ic)
{
    struct ieee80211vap *vap;
    vap = TAILQ_FIRST(&ic->ic_vaps);
    vap->iv_evtable->wlan_bringup_ap_vaps(vap->iv_ifp);
    return;
}

#define IEEE80211_CHECK_IE_SIZE(__x, __y, __z) \
        (__x < sizeof(struct __z) || \
        (__y->length != (sizeof(struct __z) - sizeof(struct __y))))
/* Process CSA/ECSA IE and switch to new announced channel */
int
ieee80211_process_csa_ecsa_ie (
    struct ieee80211_node *ni,
    struct ieee80211_action * pia,
    uint32_t frm_len
    )
{
    struct ieee80211_extendedchannelswitch_ie * pecsaIe = NULL;
    struct ieee80211_ath_channelswitch_ie * pcsaIe = NULL;
    struct ieee80211_ie_sec_chan_offset *psecchanoffsetIe = NULL;
    struct ieee80211_ie_wide_bw_switch  *pwidebwie = NULL;
    struct ieee80211_ie_header *ie_header = NULL;
    u_int8_t *cswrap = NULL;


    struct ieee80211vap *vap = ni->ni_vap;
     struct ieee80211com *ic = ni->ni_ic;

    struct ieee80211_ath_channel* chan = NULL;

    u_int8_t * ptmp1 = NULL;
    int      err = (-EINVAL);

    ASSERT(pia);

    if(!(ic->ic_flags_ext & IEEE80211_FEXT_MARKDFS)){
        return EOK;
        /*Returning EOK to make sure that we dont get disconnect from AP */
    }
    ptmp1 = (u_int8_t *)pia + sizeof(struct ieee80211_action);

    if ((*ptmp1 != IEEE80211_ELEMID_CHANSWITCHANN) &&
            (*ptmp1 != IEEE80211_ELEMID_EXTCHANSWITCHANN)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Wrong IE [%d] received\n",
             __func__, *ptmp1);
        /* unknown CSA Action frame format, but do not disconnect immediately.
           Wait for CSA to be processed correctly in beacons if possible. */
        return EOK;
    }

    /* Find CSA/ECSA/SECOFFSET/WIDEBW IEs from received action frame */
    while (frm_len > 0)
    {
        if (frm_len <= sizeof(struct ieee80211_ie_header))
            break;
        ie_header = (struct ieee80211_ie_header *)ptmp1;

        if (ie_header->length == 0)
            break;

        switch (ie_header->element_id) {
        case IEEE80211_ELEMID_CHANSWITCHANN:
            /* Sanity check for size. */
            if (IEEE80211_CHECK_IE_SIZE(frm_len, ie_header, ieee80211_ath_channelswitch_ie)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s: Invalid IE (%d) size\n",
                        __func__, ie_header->element_id);
                return EOK;
            }
            pcsaIe = (struct ieee80211_ath_channelswitch_ie *)ptmp1;
            ptmp1 += sizeof(struct ieee80211_ath_channelswitch_ie);
            frm_len -= sizeof(struct ieee80211_ath_channelswitch_ie);
            break;
        case IEEE80211_ELEMID_EXTCHANSWITCHANN:
            /* Sanity check for size. */
            if (IEEE80211_CHECK_IE_SIZE(frm_len, ie_header, ieee80211_extendedchannelswitch_ie)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s: Invalid IE (%d) size\n",
                        __func__, ie_header->element_id);
                return EOK;
            }
            pecsaIe = (struct ieee80211_extendedchannelswitch_ie *)ptmp1;
            ptmp1 += sizeof(struct ieee80211_extendedchannelswitch_ie);
            frm_len -= sizeof(struct ieee80211_extendedchannelswitch_ie);
            break;
        case IEEE80211_ELEMID_SECCHANOFFSET:
            /* Sanity check for size. */
            if (IEEE80211_CHECK_IE_SIZE(frm_len, ie_header, ieee80211_ie_sec_chan_offset)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s: Invalid IE (%d) size\n",
                        __func__, ie_header->element_id);
                return EOK;
            }
            psecchanoffsetIe = (struct ieee80211_ie_sec_chan_offset *)ptmp1;
            ptmp1 += sizeof(struct ieee80211_ie_sec_chan_offset);
            frm_len -= sizeof(struct ieee80211_ie_sec_chan_offset);
            break;
        case IEEE80211_ELEMID_WIDE_BAND_CHAN_SWITCH:
            /* Sanity check for size. */
            if (IEEE80211_CHECK_IE_SIZE(frm_len, ie_header, ieee80211_ie_wide_bw_switch)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s: Invalid IE (%d) size\n",
                        __func__, ie_header->element_id);
                return EOK;
            }
            pwidebwie = (struct ieee80211_ie_wide_bw_switch *)ptmp1;
            ptmp1 += sizeof(struct ieee80211_ie_wide_bw_switch);
            frm_len -= sizeof(struct ieee80211_ie_wide_bw_switch);
            break;
        case IEEE80211_ELEMID_CHAN_SWITCH_WRAP:
            /* support is only for WIDEBW IE, iterating over the wrapper for now */
            cswrap = ptmp1;
            ptmp1 += sizeof(struct ieee80211_ie_header);
            frm_len -=  sizeof(struct ieee80211_ie_header);
            break;
        case IEEE80211_ELEMID_VHT_TX_PWR_ENVLP:
        case 118: /* MESH CHANNEL SWITCH PARAMETERS */
        case IEEE80211_ELEMID_COUNTRY:
            /* These IEs maybe received but are not processed for channel switch. iterating */
            ptmp1 += (ie_header->length + sizeof(struct ieee80211_ie_header));
            frm_len -= (ie_header->length + sizeof(struct ieee80211_ie_header));
            break;
        default:
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "%s: Invalid IE (%d) in CSA ACTION\n",
                    __func__, ie_header->element_id);
            return EOK;
            break;
        } /* End of switch case */
    }

    chan = ieee80211_get_new_sw_chan (ni, pcsaIe, pecsaIe, psecchanoffsetIe, pwidebwie, cswrap);
    ieee80211_mgmt_sta_send_csa_rx_nl_msg(ic, ni, chan, pcsaIe, pecsaIe, psecchanoffsetIe, pwidebwie);

    if(!chan)
        return EOK;

     /*
     * Set or clear flag indicating reception of channel switch announcement
     * in this channel. This flag should be set before notifying the scan
     * algorithm.
     * We should not send probe requests on a channel on which CSA was
     * received until we receive another beacon without the said flag.
     */
    if ((pcsaIe != NULL) || (pecsaIe != NULL)) {
        ic->ic_curchan->ic_flagext |= IEEE80211_CHAN_CSA_RECEIVED;
    }

#if ATH_SUPPORT_IBSS_DFS
    /* only support pcsaIe for now */
    if (vap->iv_opmode == IEEE80211_M_IBSS &&
        chan &&
        pcsaIe) {

        IEEE80211_RADAR_FOUND_LOCK(ic);
        if(ieee80211_dfs_action(vap, pcsaIe, false)) {
            err = EOK;
        }
        IEEE80211_RADAR_FOUND_UNLOCK(ic);

    } else if (chan && (pcsaIe || pecsaIe)) {
#else
    if (chan && (pcsaIe || pecsaIe)) {
#endif /* ATH_SUPPORT_IBSS_DFS */
        /*
         * For Station, just switch channel right away.
         */
        if (!IEEE80211_IS_CHAN_SWITCH_STARTED(ic) &&
            (chan != vap->iv_bsschan))
        {
            if (pcsaIe)
                    ni->ni_chanswitch_tbtt = pcsaIe->tbttcount;
            else if (pecsaIe)
                    ni->ni_chanswitch_tbtt = pecsaIe->tbttcount;

                    /*
             * Issue a channel switch request to resource manager.
             * If the function returns EOK (0) then its ok to change the channel synchronously
             * If the function returns EBUSY then resource manager will
             * switch channel asynchronously and post an event event handler registred by vap and
             * vap handler will inturn do the rest of the processing involved.
                     */
            err = ieee80211_resmgr_request_chanswitch(ic->ic_resmgr, vap, chan, MLME_REQ_ID);

            if (err == EOK) {
                /*
                 * Start channel switch timer to change the channel
                 */
                IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                               "%s: Received CSA action frame \n",__func__);
                ni->ni_capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
                ieee80211_process_csa(ni, chan, pcsaIe, pecsaIe, NULL);
            } else if (err == EBUSY) {
                err = EOK;
            }
        }
    }
    IEEE80211_NOTE(vap, IEEE80211_MSG_ACTION, ni,
                   "%s: Exited.\n",__func__
                   );

    return err;
}

#if ATH_SUPPORT_IBSS_DFS

static int
ieee80211_validate_ie(
    u_int8_t * pucInfoBlob,
    u_int32_t uSizeOfBlob,
    u_int8_t ucInfoId,
    u_int8_t * pucLength,
    u_int8_t ** ppvInfoEle
    )
{
    int status = IEEE80211_STATUS_SUCCESS;
    struct ieee80211_ie_header * pInfoEleHdr = NULL;
    u_int32_t uRequiredSize = 0;
    bool bFound = FALSE;

    *pucLength = 0;
    *ppvInfoEle = NULL;
    while(uSizeOfBlob) {

        pInfoEleHdr = (struct ieee80211_ie_header *)pucInfoBlob;
        if (uSizeOfBlob < sizeof(struct ieee80211_ie_header)) {
            break;
        }

        uRequiredSize = (u_int32_t)(pInfoEleHdr->length) + sizeof(struct ieee80211_ie_header);
        if (uSizeOfBlob < uRequiredSize) {
            break;
        }

        if (pInfoEleHdr->element_id == ucInfoId) {
            *pucLength = pInfoEleHdr->length;
            *ppvInfoEle = pucInfoBlob + sizeof(struct ieee80211_ie_header);
            bFound = TRUE;
            break;
        }

        uSizeOfBlob -= uRequiredSize;
        pucInfoBlob += uRequiredSize;
    }

    if (!bFound) {
        status = IEEE80211_REASON_IE_INVALID;
    }
    return status;
}

/*
 * when we enter this function, we assume:
 * 1. if pmeasrepie is NULL, we build a measurement report with map field's radar set to 1
 * 2. if pmeasrepie is not NULL, only job is to repeat it.
 * 3. only basic report is used for now. For future expantion, it should be modified.
 *
 */
int
ieee80211_measurement_report_action (
    struct ieee80211vap                    *vap,
    struct ieee80211_measurement_report_ie *pmeasrepie
    )
{
    struct ieee80211_action_mgt_args       *actionargs;
    struct ieee80211_measurement_report_ie *newmeasrepie;
    struct ieee80211_measurement_report_basic_report *pbasic_report;
    struct ieee80211com *ic = vap->iv_ic;

    /* currently we only support IBSS mode */
    if (vap->iv_opmode != IEEE80211_M_IBSS) {
        return -EINVAL;
    }

    if(vap->iv_measrep_action_count_per_tbtt > vap->iv_ibss_dfs_csa_measrep_limit) {
        return EOK;
    }
    vap->iv_measrep_action_count_per_tbtt++;

    actionargs = OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(struct ieee80211_action_mgt_args) , GFP_KERNEL);
    if (actionargs == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Unable to alloc arg buf. Size=%d\n",
                                    __func__, sizeof(struct ieee80211_action_mgt_args));
        return -EINVAL;
    }
    OS_MEMZERO(actionargs, sizeof(struct ieee80211_action_mgt_args));


    if (pmeasrepie) {

        actionargs->category = IEEE80211_ACTION_CAT_SPECTRUM;
        actionargs->action   = IEEE80211_ACTION_MEAS_REPORT;
        actionargs->arg1     = 0;   /* Dialog Token */
        actionargs->arg2     = 0;   /* not used */
        actionargs->arg3     = 0;   /* not used */
        actionargs->arg4     = (u_int8_t *)pmeasrepie;
        ieee80211_send_action(vap->iv_bss, actionargs, NULL);
    } else {

        newmeasrepie = OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(struct ieee80211_measurement_report_ie) , GFP_KERNEL);
        if (newmeasrepie == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Unable to alloc report ie buf. Size=%d\n",
                                        __func__, sizeof(struct ieee80211_measurement_report_ie));
            OS_FREE(actionargs);
            return -EINVAL;
        }
        OS_MEMZERO(newmeasrepie, sizeof(struct ieee80211_measurement_report_ie));
        newmeasrepie->ie = IEEE80211_ELEMID_MEASREP;
        newmeasrepie->len = sizeof(struct ieee80211_measurement_report_ie)- sizeof(struct ieee80211_ie_header); /* might be different if other report type is supported */
        newmeasrepie->measurement_token = 0;
        newmeasrepie->measurement_report_mode = 0;
        newmeasrepie->measurement_type = 0;
        pbasic_report = (struct ieee80211_measurement_report_basic_report *)newmeasrepie->pmeasurement_report;

        pbasic_report->channel = ic->ic_curchan->ic_ieee;
        pbasic_report->measurement_start_time = vap->iv_bss->ni_tstamp.tsf; /* just make one */
        pbasic_report->measurement_duration = 50; /* fake */
        pbasic_report->map.radar = 1;
        pbasic_report->map.unmeasured = 0;

        actionargs->category = IEEE80211_ACTION_CAT_SPECTRUM;
        actionargs->action   = IEEE80211_ACTION_MEAS_REPORT;
        actionargs->arg1     = 0;   /* Dialog Token */
        actionargs->arg2     = 0;   /* not used */
        actionargs->arg3     = 0;   /* not used */
        actionargs->arg4     = (u_int8_t *)newmeasrepie;
        ieee80211_send_action(vap->iv_bss, actionargs, NULL);

        OS_FREE(newmeasrepie);
    }

    OS_FREE(actionargs);

    /* trigger beacon_update timer, we use it as timer */
    OS_DELAY(500);
    ieee80211_ibss_beacon_update_start(ic);

    return EOK;
}


/*  Process Measurement report and take action
 *  Currently this is only supported/tested in IBSS_DFS situation.
 *
 */

int
ieee80211_process_meas_report_ie (
    struct ieee80211_node *ni,
    struct ieee80211_action * pia
    )
{
    struct ieee80211_measurement_report_ie *pmeasrepie = NULL;
    struct ieee80211_measurement_report_basic_report *pbasic_report =NULL;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t * ptmp1 = NULL;
    u_int8_t * ptmp2 = NULL;
    u_int8_t size;
    int      err = (-EOK);
    u_int8_t i = 0;
    u_int8_t unit_len         = sizeof(struct channel_map_field);

    ASSERT(pia);

    if(!(ic->ic_flags_ext & IEEE80211_FEXT_MARKDFS)) {
        return err;
    }
    ptmp1 = (u_int8_t *)pia + sizeof(struct ieee80211_action_measrep_header);

    /*Find measurement report IE*/
    if (ieee80211_validate_ie((u_int8_t *)ptmp1,
                        sizeof(struct ieee80211_measurement_report_ie),
                        IEEE80211_ELEMID_MEASREP,
                        &size,
                        (u_int8_t **)&ptmp2) == IEEE80211_STATUS_SUCCESS)
    {
           ptmp2 -= sizeof(struct ieee80211_ie_header);
           pmeasrepie = (struct ieee80211_measurement_report_ie * )ptmp2 ;
    } else {
        err = -EINVAL;
        return err;
    }

    if (vap->iv_opmode == IEEE80211_M_IBSS) {

        if(pmeasrepie->measurement_type == 0) { /* basic report */
            pbasic_report = (struct ieee80211_measurement_report_basic_report *)pmeasrepie->pmeasurement_report;
        } else {
            err = -EINVAL;
            return err;
        }

         /* mark currnet DFS element's channel's map field */
        for( i = (vap->iv_ibssdfs_ie_data.len - IBSS_DFS_ZERO_MAP_SIZE)/unit_len; i >0; i--) {
            if (vap->iv_ibssdfs_ie_data.ch_map_list[i-1].ch_num == pbasic_report->channel) {
                vap->iv_ibssdfs_ie_data.ch_map_list[i-1].chmap_in_byte |= pbasic_report->chmap_in_byte;
                vap->iv_ibssdfs_ie_data.ch_map_list[i-1].ch_map.unmeasured = 0;
                break;
            }
        }

        if(ic->ic_curchan->ic_ieee == pbasic_report->channel) {
            if (IEEE80211_ADDR_EQ(vap->iv_ibssdfs_ie_data.owner, vap->iv_myaddr) &&
                pbasic_report->map.radar) {
                IEEE80211_RADAR_FOUND_LOCK(ic);
                ieee80211_dfs_action(vap, NULL, false);
                IEEE80211_RADAR_FOUND_UNLOCK(ic);
            } else if (pbasic_report->map.radar) { /* Not owner, trigger recovery mode */
                if (vap->iv_ibssdfs_state == IEEE80211_IBSSDFS_JOINER) {
                    vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_WAIT_RECOVERY;
                }
            }
        }
        /* repeat measurement report when we receive it */
        err = ieee80211_measurement_report_action(vap, pmeasrepie);
    }
    return err;
}
#endif /* ATH_SUPPORT_IBSS_DFS */

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
u_int8_t *
ieee80211_add_mmie(struct ieee80211vap *vap, u_int8_t *bfrm, u_int32_t len)
{
    return wlan_crypto_add_mmie(vap->vdev_obj, bfrm, len);
}
#else
u_int8_t *
ieee80211_add_mmie(struct ieee80211vap *vap, u_int8_t *bfrm, u_int32_t len)
{
    struct ieee80211_key *key = &vap->iv_igtk_key;
    struct ieee80211_ath_mmie *mmie;
    u_int8_t *pn, *aad, *efrm, nounce[12];
    struct ieee80211_frame *wh;
    u_int32_t i, hdrlen, aad_len;
    u_int8_t* buf;

    if (!key && !bfrm) {
        /* Invalid Key or frame */
        return NULL;
    }

    IEEE80211_CRYPTO_KEY_LOCK_BH(key);
    if (!key->wk_private) {
         IEEE80211_CRYPTO_KEY_UNLOCK_BH(key);
         return NULL;
    }


    efrm = bfrm + len;
    len += sizeof(*mmie);
    hdrlen = sizeof(*wh);
    /* aad_len 20 = fc(2) + 3 *addr(18) */
    aad_len = 20;

    mmie = (struct ieee80211_ath_mmie *) efrm;
    mmie->element_id = IEEE80211_ELEMID_MMIE;
    mmie->length = sizeof(*mmie) - 2;
    mmie->key_id = cpu_to_le16(key->wk_keyix);

    /* PN = PN + 1 */
    pn = (u_int8_t*)&key->wk_keytsc;

    for (i = 0; i <= 5; i++) {
        pn[i]++;
        if (pn[i])
            break;
    }

    /* Copy IPN */
    memcpy(mmie->sequence_number, pn, 6);

    buf = qdf_mem_malloc(len - hdrlen + aad_len);
    if (!buf) {
        qdf_print("%s[%d] malloc failed", __func__, __LINE__);
        IEEE80211_CRYPTO_KEY_UNLOCK_BH(key);
        return NULL;
    }
    qdf_mem_zero(buf, len - hdrlen + aad_len);
    aad = buf;

    wh = (struct ieee80211_frame *) bfrm;

    /* generate BIP AAD: FC(masked) || A1 || A2 || A3 */

    /* FC type/subtype */
    aad[0] = wh->i_fc[0];
    /* Mask FC Retry, PwrMgt, MoreData flags to zero */
    aad[1] = wh->i_fc[1] & ~(IEEE80211_FC1_RETRY | IEEE80211_FC1_PWR_MGT | IEEE80211_FC1_MORE_DATA);
    /* A1 || A2 || A3 */
    memcpy(aad + 2, wh->i_addr1, IEEE80211_ADDR_LEN);
    memcpy(aad + 8, wh->i_addr2, IEEE80211_ADDR_LEN);
    memcpy(aad + 14, wh->i_addr3, IEEE80211_ADDR_LEN);
   /* Init 16 byte mic buffer with zero */
    qdf_mem_zero(mmie->mic, 16);
    /*
     * MIC = AES-128-CMAC(IGTK, AAD || Management Frame Body || MMIE, 64)
     */

    if((RSN_CIPHER_IS_CMAC(&vap->iv_rsn)) || RSN_CIPHER_IS_CMAC256(&vap->iv_rsn)){
        /* size of MIC is 16 for GMAC, where size of mic for CMAC is 8 */
        len -= 8;
        mmie->length -= 8;
        ieee80211_cmac_calc_mic(key, aad, bfrm + hdrlen, len - hdrlen, mmie->mic);
    }else if((RSN_CIPHER_IS_GMAC(&vap->iv_rsn)) || RSN_CIPHER_IS_GMAC256(&vap->iv_rsn)){
        memcpy(nounce, wh->i_addr2, IEEE80211_ADDR_LEN);
        /* swap pn number in nounce */
        nounce[6] = pn[5];
        nounce[7] = pn[4];
        nounce[8] = pn[3];
        nounce[9] = pn[2];
        nounce[10] = pn[1];
        nounce[11] = pn[0];

        qdf_mem_copy(buf + aad_len, (bfrm + hdrlen), len - hdrlen);

        ieee80211_gmac_calc_mic(key, aad, NULL, (len - hdrlen + aad_len), mmie->mic, nounce);
    } else {
        qdf_mem_free(buf);
        IEEE80211_CRYPTO_KEY_UNLOCK_BH(key);
        return NULL;
    }
    qdf_mem_free(buf);

    IEEE80211_CRYPTO_KEY_UNLOCK_BH(key);
    return bfrm + len;

}
#endif /* end of  #ifdef WLAN_CONV_CRYPTO_SUPPORTED */

void
ieee80211_set_vht_rates(struct ieee80211com *ic, struct ieee80211vap  *vap)
{
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap),
             rx_streams = ieee80211_get_rxstreams(ic, vap);
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    u_int8_t bcc_11ng_256qam_20mhz_s = 0;
    uint32_t iv_ldpc = vap->vdev_mlme->proto.generic.ldpc;

    bcc_11ng_256qam_20mhz_s = IEEE80211_IS_CHAN_11NG(ic->ic_curchan) &&
                                ieee80211_vap_256qam_is_set(vap) &&
                                (iv_ldpc == IEEE80211_HTCAP_C_LDPC_NONE) &&
                                (ic_cw_width == IEEE80211_CWM_WIDTH20);
    /* Adjust supported rate set based on txchainmask */
    switch (tx_streams) {
        default:
            /* Default to single stream */
        case 1:
             /*MCS9 is not supported for BCC, NSS=1,2 in 20Mhz */
            /* if ldpc disabled, then allow upto MCS 8 */
            if(bcc_11ng_256qam_20mhz_s && !iv_ldpc) {
                 vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = VHT_MCSMAP_NSS1_MCS0_8; /* MCS 0-8 */
            }
            else {
                 vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = VHT_MCSMAP_NSS1_MCS0_9; /* MCS 0-9 */
            }
            if (vap->iv_vht_tx_mcsmap) {
                vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS1_MASK);
            }
        break;

        case 2:
            /* Dual stream */
             /*MCS9 is not supported for BCC, NSS=1,2 in 20Mhz */
            /* if ldpc disabled, then allow upto MCS 8 */
            if(bcc_11ng_256qam_20mhz_s && !iv_ldpc) {
                 vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = VHT_MCSMAP_NSS2_MCS0_8; /* MCS 0-8 */
            }
            else {
                 vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = VHT_MCSMAP_NSS2_MCS0_9; /* MCS 0-9 */
            }
            if (vap->iv_vht_tx_mcsmap) {
                vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS2_MASK);
            }
        break;

        case 3:
            /* Tri stream */
            vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = VHT_MCSMAP_NSS3_MCS0_9; /* MCS 0-9 */
            if (vap->iv_vht_tx_mcsmap) {
                vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS3_MASK);
            }
        break;
        case 4:
        /* four stream */
             /*MCS9 is not supported for BCC, NSS=1,2,4 in 20Mhz */
            /* if ldpc disabled, then allow upto MCS 8 */
            if(bcc_11ng_256qam_20mhz_s && !iv_ldpc) {
                 vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = VHT_MCSMAP_NSS4_MCS0_8; /* MCS 0-8 */
            }
            else {
                 vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map = VHT_MCSMAP_NSS4_MCS0_9; /* MCS 0-9 */
            }
            if (vap->iv_vht_tx_mcsmap) {
            vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS4_MASK);
            }
        break;
#if QCA_SUPPORT_5SS_TO_8SS
       /* 8 chain TODO: As of QCA8074, VHT in 2.4 GHz for NSS > 4 is not
        * supported. Hence this case is not treated separately. However, if
        * future chipsets add support, then add required processing for
        * tx_streams values 5-8 according to the applicable LDPC capabilities.
        */
        case 5:
        /* five stream */
            vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS5_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_tx_mcsmap) {
                vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                    (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS5_MASK);
            }
        break;

        case 6:
        /* six stream */
            vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS6_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_tx_mcsmap) {
                vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                    (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS6_MASK);
            }
        break;

        case 7:
        /* seven stream */
            vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS7_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_tx_mcsmap) {
                vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                    (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS7_MASK);
            }
        break;

        case 8:
        /* eight stream */
            vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS8_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_tx_mcsmap) {
                vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map =
                    (vap->iv_vht_tx_mcsmap | VHT_MCSMAP_NSS8_MASK);
            }
        break;
#endif /* QCA_SUPPORT_5SS_TO_8SS */
    } /* end switch */

    /* Adjust rx rates based on the rx chainmask */
    switch (rx_streams) {
        default:
            /* Default to single stream */
        case 1:
             /*MCS9 is not supported for BCC, NSS=1,2 in 20Mhz */
            /* if ldpc disabled, then allow upto MCS 8 */
            if(bcc_11ng_256qam_20mhz_s && !iv_ldpc) {
                 vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = VHT_MCSMAP_NSS1_MCS0_8; /* MCS 0-8 */
            }
            else {
                 vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = VHT_MCSMAP_NSS1_MCS0_9;
            }
            if (vap->iv_vht_rx_mcsmap) {
                vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS1_MASK);
            }
        break;

        case 2:
            /* Dual stream */
             /*MCS9 is not supported for BCC, NSS=1,2 in 20Mhz */
            /* if ldpc disabled, then allow upto MCS 8 */
            if(bcc_11ng_256qam_20mhz_s && !iv_ldpc) {
                 vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = VHT_MCSMAP_NSS2_MCS0_8; /* MCS 0-8 */
            }
            else {
                 vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = VHT_MCSMAP_NSS2_MCS0_9;
            }
            if (vap->iv_vht_rx_mcsmap) {
                vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS2_MASK);
            }
        break;

        case 3:
            /* Tri stream */
            vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = VHT_MCSMAP_NSS3_MCS0_9;
            if (vap->iv_vht_rx_mcsmap) {
                vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS3_MASK);
            }
        break;
        case 4:
        /* four stream */
             /*MCS9 is not supported for BCC, NSS=1,2 in 20Mhz */
            /* if ldpc disabled, then allow upto MCS 8 */
            if(bcc_11ng_256qam_20mhz_s && !iv_ldpc) {
                 vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = VHT_MCSMAP_NSS4_MCS0_8; /* MCS 0-8 */
            }
            else {
                 vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map = VHT_MCSMAP_NSS4_MCS0_9;
            }
            if (vap->iv_vht_rx_mcsmap) {
            vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS4_MASK);
            }
        break;
#if QCA_SUPPORT_5SS_TO_8SS
       /* 8 chain TODO: As of QCA8074, VHT in 2.4 GHz for NSS > 4 is not
        * supported. Hence this case is not treated separately. However, if
        * future chipsets add support, then add required processing for
        * tx_streams values 5-8 according to the applicable LDPC capabilities.
        */
        case 5:
        /* five stream */
            vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS5_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_rx_mcsmap) {
                vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                    (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS5_MASK);
            }
        break;

        case 6:
        /* six stream */
            vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS6_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_rx_mcsmap) {
                vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                    (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS6_MASK);
            }
        break;

        case 7:
        /* seven stream */
            vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS7_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_rx_mcsmap) {
                vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                    (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS7_MASK);
            }
        break;

        case 8:
        /* eight stream */
            vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                VHT_MCSMAP_NSS8_MCS0_9; /* MCS 0-9 */

            if (vap->iv_vht_rx_mcsmap) {
                vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map =
                    (vap->iv_vht_rx_mcsmap | VHT_MCSMAP_NSS8_MASK);
            }
        break;
#endif /* QCA_SUPPORT_5SS_TO_8SS */
    }
    if(vap->iv_configured_vht_mcsmap) {
        /* rx and tx vht mcs will be same for iv_configured_vht_mcsmap option */
        /* Assign either rx or tx mcs map back to this variable so that iwpriv get operation prints exact value */
        vap->iv_configured_vht_mcsmap = vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map;
    }
    vap->iv_set_vht_mcsmap = true;
}


/*
 * This function updates the VHTCAP based on the
 * MU-CAP WAR variable status
 */
static inline u_int32_t mu_cap_war_probe_response_change(u_int32_t vhtcap_info,
                                             u_int8_t subtype,
                                             struct ieee80211vap  *vap,
                                             struct ieee80211_node *ni,
                                             struct ieee80211com *ic,
                                             u_int8_t *sta_mac_addr,
                                             MU_CAP_WAR *war)
{
    struct DEDICATED_CLIENT_MAC *dedicated_mac = NULL;
    int hash;
    u_int32_t vhtcap_info_modified = vhtcap_info;
    if (!war->mu_cap_war ||
        !war->modify_probe_resp_for_dedicated ||
        !ni->dedicated_client ||
        (subtype !=  IEEE80211_FC0_SUBTYPE_PROBE_RESP) ||
        (!(vhtcap_info&IEEE80211_VHTCAP_MU_BFORMER))) {

        /*
         * No need to change the existing VHT-CAP
         * or do any further processing
         */
        return vhtcap_info;
    }

    /*
     * Hacking the VHT-CAP so that
     * the dedicated client joins as SU-2x2
     */
    vhtcap_info_modified &= ~IEEE80211_VHTCAP_MU_BFORMER;

    if (sta_mac_addr == NULL) {
        ieee80211_note(vap, IEEE80211_MSG_ANY,
                      "ERROR!!! NULL STA_MAC_ADDR Passed to %s\n",
                      __func__);
        return vhtcap_info;
    }

    hash = IEEE80211_NODE_HASH(sta_mac_addr);
    LIST_FOREACH(dedicated_mac, &war->dedicated_client_list[hash], list) {
        if (IEEE80211_ADDR_EQ(dedicated_mac->macaddr, sta_mac_addr)) {

            /*
             * Entry already present, no need to add again
             */
            return vhtcap_info_modified;
        }
    }


    /*
     * Have at the most, twice as many floating ProbeResponse-hacked clients
     * as the number of clients supported
     * The multiply-by-2 is an arbitrary ceiling limit. To remove
     * this limit, there should be timer logic to periodically flush
     * out old Probe-Response-hacked clients from the database.
     * At this point this does not seem to be a priority.
     */
    if (war->dedicated_client_number >= (MAX_PEER_NUM*2)) {
        ieee80211_note(vap, IEEE80211_MSG_ANY,
                "ERROR!! Too many floating PR clients, we might run out of %s",
                "memory if we keep adding these clients to DB\n");
        return vhtcap_info;
    }

    /*
     * Maintain the list of clients to which
     * we send this 'hacked' VHTCAP in probe response
     */
    dedicated_mac =
        OS_MALLOC(ic->ic_osdev, sizeof(*dedicated_mac), 0);
    if (dedicated_mac == NULL) {
        ieee80211_note(vap, IEEE80211_MSG_ANY, "ERROR!! Memory allocation failed in %s\n",
                __func__);
        return vhtcap_info;
    }
    OS_MEMSET(dedicated_mac, 0, sizeof(*dedicated_mac));
    OS_MEMCPY(dedicated_mac->macaddr, sta_mac_addr,
            sizeof(dedicated_mac->macaddr));
    war->dedicated_client_number++;
    LIST_INSERT_HEAD(&war->dedicated_client_list[hash], dedicated_mac, list);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
            "Adding %s to modified probe-resp list\n",
                    ether_sprintf(sta_mac_addr));
    return vhtcap_info_modified;
}

u_int8_t *
ieee80211_add_vhtcap(u_int8_t *frm, struct ieee80211_node *ni,
                     struct ieee80211com *ic, u_int8_t subtype,
                     struct ieee80211_framing_extractx *extractx,
                     u_int8_t *sta_mac_addr)
{
    int vhtcaplen = sizeof(struct ieee80211_ie_vhtcap);
    struct ieee80211_ie_vhtcap *vhtcap = (struct ieee80211_ie_vhtcap *)frm;
    struct ieee80211vap  *vap = ni->ni_vap;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;
    u_int32_t vhtcap_info;
    u_int32_t ni_vhtbfeestscap;
    u_int8_t rx_streams = ieee80211_get_rxstreams(ic, vap);
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap);
    ieee80211_vht_rate_t ni_tx_vht_rates;
    struct supp_tx_mcs_extnss tx_mcs_extnss_cap;
    u_int16_t temp;
    struct ieee80211_bwnss_map nssmap;
    u_int8_t rx_chainmask = ieee80211com_get_rx_chainmask(ic);
    enum ieee80211_phymode cur_mode;

    qdf_mem_zero(&nssmap, sizeof(nssmap));

    vhtcap->elem_id   = IEEE80211_ELEMID_VHTCAP;
    vhtcap->elem_len  = sizeof(struct ieee80211_ie_vhtcap) - 2;

    /* Choose between the STA from advertising negotiated channel width
     * caps and advertising it's desired channel width caps */
    if (vap->iv_sta_max_ch_cap) {
        cur_mode = vap->iv_des_mode;
    } else {
        cur_mode = vap->iv_cur_mode;
    }

    /* Fill in the VHT capabilities info */
    vhtcap_info = ic->ic_vhtcap;

    /* Use firmware short GI capability if short GI is enabled on this vap.
     * Else clear short GI for both 80 and 160 MHz.
     */
    if (!vap->iv_sgi) {
        vhtcap_info &= ~(IEEE80211_VHTCAP_SHORTGI_80 | IEEE80211_VHTCAP_SHORTGI_160);
    }

    vhtcap_info &= ((vdev_mlme->proto.generic.ldpc & IEEE80211_HTCAP_C_LDPC_RX) ?
            ic->ic_vhtcap  : ~IEEE80211_VHTCAP_RX_LDPC);


    if ((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ) ||
        (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) {
        /* Check negotiated Rx NSS */
        ieee80211_get_vht_rates(ni->ni_rx_vhtrates, &ni_tx_vht_rates);
        rx_streams = MIN(rx_streams, ni_tx_vht_rates.num_streams);
    }

    /* Adjust the TX and RX STBC fields based on the chainmask and config status */
    vhtcap_info &= (((vap->iv_tx_stbc) && (tx_streams > 1)) ?  ic->ic_vhtcap : ~IEEE80211_VHTCAP_TX_STBC);
    vhtcap_info &= (((vap->iv_rx_stbc) && (rx_streams > 0)) ?  ic->ic_vhtcap : ~IEEE80211_VHTCAP_RX_STBC);

    /* Support WFA R1 test case -- beamformer & nss =1*/
    if(tx_streams > 1)
    {
        vhtcap_info &= ((vdev_mlme->proto.vht_info.subfer) ?  ic->ic_vhtcap : ~IEEE80211_VHTCAP_SU_BFORMER);
    }else{
        vhtcap_info &= (((vdev_mlme->proto.vht_info.subfer) && (vdev_mlme->proto.vht_info.subfee)
                         && (vdev_mlme->proto.vht_info.mubfer == 0)
                         &&(vdev_mlme->proto.vht_info.mubfee == 0) && (vdev_mlme->proto.generic.nss == 1)) ?
                         ic->ic_vhtcap : ~IEEE80211_VHTCAP_SU_BFORMER);
    }
    vhtcap_info &= ((vdev_mlme->proto.vht_info.subfee) ?  ic->ic_vhtcap : ~IEEE80211_VHTCAP_SU_BFORMEE);



    /* if SU Beamformer/Beamformee is not set then MU Beamformer/Beamformee need to be disabled */
    vhtcap_info &= (((vdev_mlme->proto.vht_info.subfer) && (vdev_mlme->proto.vht_info.mubfer)
                && (tx_streams > 1)) ?  ic->ic_vhtcap : ~IEEE80211_VHTCAP_MU_BFORMER);
    vhtcap_info = mu_cap_war_probe_response_change(vhtcap_info, subtype,
                                                   vap, ni, ic, sta_mac_addr,
                                                   &vap->iv_mu_cap_war);
    ni->dedicated_client = 0;

    /* For Lithium chipsets and above, don't limit/disable
     * MUBFEE capability based on total no of of rxstreams
     */
    if(ic->ic_no_bfee_limit) {
        vhtcap_info &= (((vdev_mlme->proto.vht_info.subfee)
                        && (vdev_mlme->proto.vht_info.mubfee)) ?
                        ic->ic_vhtcap : ~IEEE80211_VHTCAP_MU_BFORMEE);
    } else {
        vhtcap_info &= (((vdev_mlme->proto.vht_info.subfee)
                        && (vdev_mlme->proto.vht_info.mubfee) && (rx_streams < 3)) ?
                        ic->ic_vhtcap : ~IEEE80211_VHTCAP_MU_BFORMEE);
    }

    vhtcap_info &= ~(IEEE80211_VHTCAP_STS_CAP_M << IEEE80211_VHTCAP_STS_CAP_S);
    if ((extractx != NULL) &&
        (extractx->fectx_nstscapwar_reqd == true) &&
            (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) {
        /* Using client's NSTS CAP value for assoc response */
        ni_vhtbfeestscap = ((ni->ni_vhtcap >> IEEE80211_VHTCAP_STS_CAP_S) &
                                IEEE80211_VHTCAP_STS_CAP_M );
        vhtcap_info |= (((vdev_mlme->proto.vht_info.bfee_sts_cap <= ni_vhtbfeestscap) ? vdev_mlme->proto.vht_info.bfee_sts_cap:ni_vhtbfeestscap) << IEEE80211_VHTCAP_STS_CAP_S);
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ASSOC, "%s:nsts war:: vap_stscap %#x,ni_stscap %#x,vhtcap:%#x\n",__func__, vdev_mlme->proto.vht_info.bfee_sts_cap,ni_vhtbfeestscap,vhtcap_info);
    }else {
        vhtcap_info |= vdev_mlme->proto.vht_info.bfee_sts_cap << IEEE80211_VHTCAP_STS_CAP_S;
    }
    vhtcap_info &= ~IEEE80211_VHTCAP_SOUND_DIM;
    if((vdev_mlme->proto.vht_info.subfer) && (tx_streams > 1)) {
        vhtcap_info |= ((vdev_mlme->proto.vht_info.sounding_dimension < (tx_streams - 1)) ?
                        vdev_mlme->proto.vht_info.sounding_dimension : (tx_streams - 1)) << IEEE80211_VHTCAP_SOUND_DIM_S;
    }

    /* Clear supported chanel width first */
    vhtcap_info &= ~(IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160 |
                     IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160 | IEEE80211_VHTCAP_EXT_NSS_BW_SUPPORT_MASK);

    /* Set supported chanel width as per current operating mode.
     * We don't need to check for HW/FW announced channel width
     * capability as this is already verified in service_ready_event.
     */
    if (!vap->iv_ext_nss_support) {
        if ((cur_mode == IEEE80211_MODE_11AC_VHT80_80) ||
			(cur_mode == IEEE80211_MODE_11AXA_HE80_80)) {
            vhtcap_info |= IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160;
        } else if ((cur_mode == IEEE80211_MODE_11AC_VHT160) ||
			(cur_mode == IEEE80211_MODE_11AXA_HE160)) {
            vhtcap_info |= IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160;
        }
    } else if (!ieee80211_get_bw_nss_mapping(vap, &nssmap, rx_chainmask)) {
        /* If AP supports EXT NSS Signaling, set vhtcap ie to
         * IEEE80211_EXTNSS_MAP_01_80F1_160FDOT5_80P80NONE,
         * IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5,
         * IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1 or
         * IEEE80211_EXTNSS_MAP_10_80F1_160F1_80P80NONE
         * as they are the only valid combination for our chipsets.
         */

        if ((cur_mode == IEEE80211_MODE_11AC_VHT80_80) ||
                (cur_mode == IEEE80211_MODE_11AXA_HE80_80)) {
            if (nssmap.flag == IEEE80211_NSSMAP_SAME_NSS_FOR_ALL_BW) {
                vhtcap_info |= IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1;
            } else if (nssmap.flag == IEEE80211_NSSMAP_1_2_FOR_160_AND_80_80) {
                vhtcap_info |= IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5;
            } else {
                vhtcap_info |= IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75;
            }
        } else if ((cur_mode == IEEE80211_MODE_11AC_VHT160) ||
                (cur_mode == IEEE80211_MODE_11AXA_HE160)) {
            if (nssmap.flag == IEEE80211_NSSMAP_SAME_NSS_FOR_ALL_BW) {
                vhtcap_info |= IEEE80211_EXTNSS_MAP_10_80F1_160F1_80P80NONE;
            } else if (nssmap.flag == IEEE80211_NSSMAP_1_2_FOR_160_AND_80_80) {
                vhtcap_info |= IEEE80211_EXTNSS_MAP_01_80F1_160FDOT5_80P80NONE;
            } else {
                vhtcap_info |= IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75;
            }
        }
    }

    /* We currently honor a 160 MHz association WAR request from callers only
     * for IEEE80211_FC0_SUBTYPE_PROBE_RESP and IEEE80211_FC0_SUBTYPE_ASSOC_RESP.
     */
    if ((extractx != NULL) &&
        (extractx->fectx_assocwar160_reqd == true) &&
        ((subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) ||
            (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP))) {
       /* Remove all indications of 160 MHz capability, to enable the STA to
        * associate.
        */
       vhtcap_info &= ~(IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160 |
                        IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160 |
                        IEEE80211_VHTCAP_SHORTGI_160);
    }

#if WAR_DISABLE_MU_2x2_STA
    /* If Disable MU-MIMO WAR is enabled, disable Beamformer in Assoc Resp */
    if ((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) &&
        (((ni->ni_vhtcap & IEEE80211_VHTCAP_SOUND_DIM) >> IEEE80211_VHTCAP_SOUND_DIM_S) == 1)) {
        vhtcap_info &= ~(IEEE80211_VHTCAP_SOUND_DIM | IEEE80211_VHTCAP_MU_BFORMER);
    }
#endif

    vhtcap->vht_cap_info = htole32(vhtcap_info);

    /* Fill in the VHT MCS info */
    ieee80211_set_vht_rates(ic,vap);
    vhtcap->rx_high_data_rate = htole16(vap->iv_vhtcap_max_mcs.rx_mcs_set.data_rate);
    tx_mcs_extnss_cap.tx_high_data_rate = (vap->iv_vhtcap_max_mcs.tx_mcs_set.data_rate);
    vhtcap->tx_mcs_map = htole16(vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map);
    if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ) {
        if(!((ni->ni_vhtcap & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160)||
             (ni->ni_vhtcap & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160))) {
            /* if not 160, advertise rx mcs map as per vap max */
            vhtcap->rx_mcs_map = htole16(vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map);
        } else {
            /* if 160 , advertise rx mcs map as of ni ( negotiated ap) rx mcs map*/
            vhtcap->rx_mcs_map = htole16(ni->ni_rx_vhtrates);
        }
    } else if((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) {
        vhtcap->rx_mcs_map = htole16(ni->ni_rx_vhtrates);
    } else {
        vhtcap->rx_mcs_map = htole16(vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map);
    }

    tx_mcs_extnss_cap.ext_nss_capable = !!(ic->ic_ext_nss_capable);
    tx_mcs_extnss_cap.reserved = 0;
    temp = htole16(*(u_int16_t *)&tx_mcs_extnss_cap);
    OS_MEMCPY(&vhtcap->tx_mcs_extnss_cap, &temp, sizeof(u_int16_t));
    return frm + vhtcaplen;
}

u_int8_t *
ieee80211_add_interop_vhtcap(u_int8_t *frm, struct ieee80211_node *ni,
                     struct ieee80211com *ic, u_int8_t subtype)
{
    int vht_interopcaplen = sizeof(struct ieee80211_ie_interop_vhtcap);
    struct ieee80211_ie_interop_vhtcap *vht_interopcap = (struct ieee80211_ie_interop_vhtcap *)frm;
    static const u_int8_t oui[4] = { VHT_INTEROP_OUI_BYTES, VHT_INTEROP_TYPE};

    vht_interopcap->elem_id   = IEEE80211_ELEMID_VENDOR;
    if ((subtype == IEEE80211_FC0_SUBTYPE_BEACON)||(subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) ||
                                                   (subtype == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)) {
        vht_interopcap->elem_len  = sizeof(struct ieee80211_ie_interop_vhtcap) - 2;
        vht_interopcaplen =  sizeof(struct ieee80211_ie_interop_vhtcap);
    }
    else {
        vht_interopcap->elem_len  = sizeof(struct ieee80211_ie_interop_vhtcap) - 9; /* Eliminating Vht op IE */
        vht_interopcaplen =  sizeof(struct ieee80211_ie_interop_vhtcap) - 7;
    }

    /* Fill in the VHT capabilities info */
    memcpy(&vht_interopcap->vht_interop_oui,oui,sizeof(oui));
    vht_interopcap->sub_type = ni->ni_vhtintop_subtype;
    ieee80211_add_vhtcap(frm + 7 , ni, ic, subtype, NULL, NULL); /* Vht IE location Inside Vendor specific VHT IE*/


    if ((subtype == IEEE80211_FC0_SUBTYPE_BEACON)||(subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) ||
                                                   (subtype == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)) {
       ieee80211_add_vhtop(frm + 21 , ni, ic, subtype, NULL); /* Adding Vht Op IE after Vht cap IE  inside Vendor VHT IE*/
    }
    return frm + vht_interopcaplen;
}


u_int8_t *
ieee80211_add_vhtop(u_int8_t *frm, struct ieee80211_node *ni,
                    struct ieee80211com *ic,  u_int8_t subtype,
                    struct ieee80211_framing_extractx *extractx)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_ie_vhtop *vhtop = (struct ieee80211_ie_vhtop *)frm;
    int vhtoplen = sizeof(struct ieee80211_ie_vhtop);
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    u_int8_t chwidth = 0, negotiate_bw = 0;
    struct ieee80211_bwnss_map nssmap;
    u_int8_t rx_chainmask = ieee80211com_get_rx_chainmask(ic);

    qdf_mem_zero(&nssmap, sizeof(nssmap));

    vhtop->elem_id   = IEEE80211_ELEMID_VHTOP;
    vhtop->elem_len  = sizeof(struct ieee80211_ie_vhtop) - 2;

    ieee80211_get_bw_nss_mapping(vap, &nssmap, rx_chainmask);

    if((ni->ni_160bw_requested == 1) && (IEEE80211_IS_CHAN_11AC_VHT160(ic->ic_curchan) ||
                IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan)) &&
            (ni->ni_chwidth == IEEE80211_VHTOP_CHWIDTH_80) &&
            (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) {
        /* Set negotiated BW */
        chwidth = ni->ni_chwidth;
        negotiate_bw = 1;
    } else {
       if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
           chwidth = vap->iv_chwidth;
       } else {
           chwidth = ic_cw_width;
       }
    }

    /* Fill in the VHT Operation info */
    if (chwidth == IEEE80211_CWM_WIDTH160) {
        if (vap->iv_rev_sig_160w) {
            if(IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan))
                vhtop->vht_op_chwidth = IEEE80211_VHTOP_CHWIDTH_REVSIG_80_80;
            else
                vhtop->vht_op_chwidth = IEEE80211_VHTOP_CHWIDTH_REVSIG_160;
        } else {
            if(IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan))
                vhtop->vht_op_chwidth = IEEE80211_VHTOP_CHWIDTH_80_80;
            else
                vhtop->vht_op_chwidth = IEEE80211_VHTOP_CHWIDTH_160;
        }
    }
    else if (chwidth == IEEE80211_CWM_WIDTH80)
        vhtop->vht_op_chwidth = IEEE80211_VHTOP_CHWIDTH_80;
    else
        vhtop->vht_op_chwidth = IEEE80211_VHTOP_CHWIDTH_2040;

    if (negotiate_bw == 1) {

            vhtop->vht_op_ch_freq_seg1 = vap->iv_bsschan->ic_vhtop_ch_freq_seg1;
            /* Note: This is applicable only for 80+80Mhz mode */
            vhtop->vht_op_ch_freq_seg2 = 0;
    }
    else {
        if (chwidth == IEEE80211_CWM_WIDTH160) {

            if (vap->iv_rev_sig_160w) {
                /* Our internal channel structure is in sync with
                 * revised 160 MHz signalling. So use seg1 and
                 * seg2 directly for 80_80 and 160.
                 */
                if (vap->iv_ext_nss_support && (!(nssmap.flag == IEEE80211_NSSMAP_SAME_NSS_FOR_ALL_BW))) {
                    /* If EXT NSS is enabled in driver, vht_op_ch_freq_seq2
                     * has to be populated in htinfo IE, for the combination of NSS values
                     * for 80, 160 and 80+80 MHz which our hardware supports.
                     * Exception to this is when AP is forced to come up with same NSS value for
                     * both 80+80MHz and 160MHz */
                    vhtop->vht_op_ch_freq_seg1 = vap->iv_bsschan->ic_vhtop_ch_freq_seg1;
                    vhtop->vht_op_ch_freq_seg2 = 0;
                } else {
                    vhtop->vht_op_ch_freq_seg1 =
                           vap->iv_bsschan->ic_vhtop_ch_freq_seg1;

                    vhtop->vht_op_ch_freq_seg2 =
                        vap->iv_bsschan->ic_vhtop_ch_freq_seg2;
                }
            } else {
                /* Use legacy 160 MHz signaling */
                if(IEEE80211_IS_CHAN_11AC_VHT160(ic->ic_curchan)) {
                    /* ic->ic_curchan->ic_vhtop_ch_freq_seg2 is centre
                     * frequency for whole 160 MHz.
                     */
                    vhtop->vht_op_ch_freq_seg1 =
                        vap->iv_bsschan->ic_vhtop_ch_freq_seg2;
                    vhtop->vht_op_ch_freq_seg2 = 0;
                } else {
                    /* 80 + 80 MHz */
                    vhtop->vht_op_ch_freq_seg1 =
                        vap->iv_bsschan->ic_vhtop_ch_freq_seg1;

                    vhtop->vht_op_ch_freq_seg2 =
                        vap->iv_bsschan->ic_vhtop_ch_freq_seg2;
                }
            }
       } else { /* 80MHZ or less */
            vhtop->vht_op_ch_freq_seg1 = vap->iv_bsschan->ic_vhtop_ch_freq_seg1;
            vhtop->vht_op_ch_freq_seg2 = 0;
        }
    }


    /* We currently honor a 160 MHz association WAR request from callers only for
     * IEEE80211_FC0_SUBTYPE_PROBE_RESP and IEEE80211_FC0_SUBTYPE_ASSOC_RESP, and
     * check if our current channel is for 160/80+80 MHz.
     */
    if ((!vap->iv_rev_sig_160w) && (extractx != NULL) &&
        (extractx->fectx_assocwar160_reqd == true) &&
        ((subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) ||
            (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) &&
        (IEEE80211_IS_CHAN_11AC_VHT160(ic->ic_curchan) ||
            IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan))) {
       /* Remove all indications of 160 MHz capability, to enable the STA to
        * associate.
        *
        * Besides, we set vht_op_chwidth to IEEE80211_VHTOP_CHWIDTH_80 without
        * checking if preceeding code had set it to lower value negotiated with
        * STA. This is for logical conformance with older VHT AP behaviour
        * wherein width advertised would remain constant across probe response
        * and assocation response.
        */

        /* Downgrade to 80 MHz */
        vhtop->vht_op_chwidth = IEEE80211_VHTOP_CHWIDTH_80;

        vhtop->vht_op_ch_freq_seg1 = vap->iv_bsschan->ic_vhtop_ch_freq_seg1;
        vhtop->vht_op_ch_freq_seg2 = 0;
    }

    /* Fill in the VHT Basic MCS set */
    vhtop->vhtop_basic_mcs_set =  htole16(ic->ic_vhtop_basic_mcs);

    if (vap->iv_csa_interop_bss_active) {
        vhtop->vht_op_chwidth = 0;
    }

    return frm + vhtoplen;
}

u_int8_t *
ieee80211_add_vht_txpwr_envlp(u_int8_t *frm, struct ieee80211_node *ni,
                    struct ieee80211com *ic,  u_int8_t subtype, u_int8_t is_subelement)
{
    struct ieee80211_ie_vht_txpwr_env *txpwr = (struct ieee80211_ie_vht_txpwr_env *)frm;
    int txpwr_len;
    struct ieee80211vap *vap = ni->ni_vap;
    u_int8_t max_pwr_abs;  /* Absolute value of max regulatory tx power*/
    u_int8_t max_pwr_fe;   /* Final Encoded value of regulatory tx power */
    struct ieee80211_ath_channel *channel = NULL;
    bool is_160_or_80p80_supported = false;

    if (ic->ic_modecaps &
                ((1 << IEEE80211_MODE_11AC_VHT160) |
                 (1 << IEEE80211_MODE_11AC_VHT80_80))) {
        is_160_or_80p80_supported = true;
    }

    if (is_160_or_80p80_supported) {
        txpwr_len = sizeof(struct ieee80211_ie_vht_txpwr_env) -
            (IEEE80211_VHT_TXPWR_MAX_POWER_COUNT - IEEE80211_VHT_TXPWR_NUM_POWER_SUPPORTED);
    }
    else {
        txpwr_len = sizeof(struct ieee80211_ie_vht_txpwr_env) -
            (IEEE80211_VHT_TXPWR_MAX_POWER_COUNT - (IEEE80211_VHT_TXPWR_NUM_POWER_SUPPORTED - 1));
    }

    txpwr->elem_id   = IEEE80211_ELEMID_VHT_TX_PWR_ENVLP;
    txpwr->elem_len  = txpwr_len - 2;

    if(!is_subelement) {
       channel = vap->iv_bsschan;
    }
    else {
       if(is_subelement == IEEE80211_VHT_TXPWR_IS_VENDOR_SUB_ELEMENT)
           channel = ic->ic_tx_next_ch;
       else
           channel = ic->ic_chanchange_channel;
    }
    /*
     * Max Transmit Power count = 2( 20,40 and 80MHz) and
     * Max Transmit Power units = 0 (EIRP)
     */

    if (is_160_or_80p80_supported) {
        txpwr->txpwr_info = IEEE80211_VHT_TXPWR_NUM_POWER_SUPPORTED - 1;
    }
    else {
        txpwr->txpwr_info = (IEEE80211_VHT_TXPWR_NUM_POWER_SUPPORTED -1) - 1;
    }

    /* Most architectures should use 2's complement, but we utilize an
     * encoding process that is architecture agnostic to be on the
     * safer side.
     */
    /* Tx Power is specified in 0.5dB steps 8-bit 2's complement
     * representation.
     */

    if (channel->ic_maxregpower < 0) {
        max_pwr_abs = -channel->ic_maxregpower;
        if (max_pwr_abs > MAX_ABS_NEG_PWR)
            max_pwr_fe = ~(MAX_ABS_NEG_PWR * 2) + 1;
        else
            max_pwr_fe = ~(max_pwr_abs * 2) + 1;
    } else {
        max_pwr_abs = channel->ic_maxregpower;
        if (max_pwr_abs > MAX_ABS_POS_PWR)
            max_pwr_fe = TWICE_MAX_POS_PWR;
        else
            max_pwr_fe = 2 * max_pwr_abs;
    }

    txpwr->local_max_txpwr[0] = txpwr->local_max_txpwr[1] =
    txpwr->local_max_txpwr[2] = max_pwr_fe;

    if (is_160_or_80p80_supported) {
        txpwr->local_max_txpwr[3] = max_pwr_fe;
    }

    return frm + txpwr_len;
}


/**
* @brief    Adds wide band sub element within channel switch wrapper IE.
*           If this function is to be used for 'Wide Bandwidth Channel Switch
*           element', then modifications will be required in function.
*
* @param frm        frame in which this sub element should be added
* @param ni         pointer to associated node structure
* @param ic         pointer to iee80211com
* @param subtype    frame subtype (beacon, probe resp etc.)
*
* @return pointer to post channel switch sub element
*/
u_int8_t*
ieee80211_add_vht_wide_bw_switch(u_int8_t *frm, struct ieee80211_node *ni,
                    struct ieee80211com *ic,  u_int8_t subtype, int is_vendor_ie)
{
    struct ieee80211_ie_wide_bw_switch *widebw = (struct ieee80211_ie_wide_bw_switch *)frm;
    int widebw_len = sizeof(struct ieee80211_ie_wide_bw_switch);
    u_int8_t    new_ch_width = 0;
    enum ieee80211_phymode new_phy_mode;
    u_int8_t                      next_chan;
    u_int16_t                     next_chwidth;
    struct ieee80211_ath_channel      *next_channel;

    if(is_vendor_ie)
    {
        next_chan = ic->ic_tx_next_ch->ic_ieee;
        next_chwidth = ieee80211_get_chan_width(ic->ic_tx_next_ch);
        next_channel = ic->ic_tx_next_ch;
    }
    else
    {
        next_chan = ic->ic_chanchange_chan;
        next_chwidth = ic->ic_chanchange_chwidth;
        next_channel = ic->ic_chanchange_channel;
    }

    OS_MEMSET(widebw, 0, sizeof(struct ieee80211_ie_wide_bw_switch));

    widebw->elem_id   = IEEE80211_ELEMID_WIDE_BAND_CHAN_SWITCH;
    widebw->elem_len  = widebw_len - 2;

    /* 11AX TODO: Revisit HE related operations below for drafts later than
     * 802.11ax draft 2.0
     */

    /* New channel width */
    switch(next_chwidth)
    {
        case CHWIDTH_VHT40:
            new_ch_width = IEEE80211_VHTOP_CHWIDTH_2040;
            break;
        case CHWIDTH_VHT80:
            new_ch_width = IEEE80211_VHTOP_CHWIDTH_80;
            break;
        case CHWIDTH_VHT160:
            if (IEEE80211_IS_CHAN_11AC_VHT160(next_channel) ||
                    IEEE80211_IS_CHAN_11AXA_HE160(next_channel)) {
                new_ch_width = IEEE80211_VHTOP_CHWIDTH_160;
            } else if (IEEE80211_IS_CHAN_11AC_VHT80_80(next_channel) ||
                    IEEE80211_IS_CHAN_11AXA_HE80_80(next_channel)) {
                new_ch_width = IEEE80211_VHTOP_CHWIDTH_80_80;
            } else {
                qdf_print("%s: Error: chanwidth 160 requested for a channel"
                        " which is neither 160 MHz nor 80+80 MHz,"
                        " flags: 0x%llx",
                        __func__, next_channel->ic_flags);
                qdf_assert_always(0);
            }
            break;
        default:
            printk("%s: Invalid destination channel width %d specified\n",
                    __func__, next_chwidth);
            qdf_assert_always(0);
            break;
    }

     /* Channel Center frequency 1 */
    if(new_ch_width != IEEE80211_VHTOP_CHWIDTH_2040) {

       widebw->new_ch_freq_seg1 = next_channel->ic_vhtop_ch_freq_seg1;
       widebw->new_ch_freq_seg2 = 0;
       if(new_ch_width == IEEE80211_VHTOP_CHWIDTH_80_80) {
           /* Channel Center frequency 2 */
           widebw->new_ch_freq_seg2 = next_channel->ic_vhtop_ch_freq_seg2;

       } else if(new_ch_width == IEEE80211_VHTOP_CHWIDTH_160) {
           /* Wide band IE definition in standard hasn't yet been changed
            * to align with new definition of VHT Op. Announce whole 160 MHz
            * channel centre frequency in seg1 and 0 in seg2.
            */
           widebw->new_ch_freq_seg1 = next_channel->ic_vhtop_ch_freq_seg2;
       }
    } else {
        new_phy_mode = ieee80211_chan2mode(ic->ic_chanchange_channel);

        if ((new_phy_mode == IEEE80211_MODE_11AC_VHT40PLUS) ||
                (new_phy_mode == IEEE80211_MODE_11AXA_HE40PLUS)) {
            widebw->new_ch_freq_seg1 = next_channel->ic_ieee + 2;
        } else if ((new_phy_mode == IEEE80211_MODE_11AC_VHT40MINUS) ||
                (new_phy_mode == IEEE80211_MODE_11AXA_HE40MINUS)) {
            widebw->new_ch_freq_seg1 = next_channel->ic_ieee - 2;
        }
    }
    widebw->new_ch_width = new_ch_width;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_DOTH,
         "%s: new_ch_width: %d, freq_seg1: %d, freq_seg2 %d\n", __func__,
         new_ch_width, widebw->new_ch_freq_seg1, widebw->new_ch_freq_seg2);

    return frm + widebw_len;
}

u_int8_t *
ieee80211_add_chan_switch_wrp(u_int8_t *frm, struct ieee80211_node *ni,
                    struct ieee80211com *ic,  u_int8_t subtype, u_int8_t extchswitch)
{
    struct ieee80211vap *vap = ni->ni_vap;
    u_int8_t *efrm;
    u_int8_t ie_len;
    /* preserving efrm pointer, if no sub element is present,
        Skip adding this element */
    efrm = frm;
     /* reserving 2 bytes for the element id and element len*/
    frm += 2;

    /*country element is added if it is extended channel switch*/
    if (extchswitch) {
        frm = ieee80211_add_country(frm, vap);
    }
    /*If channel width not 20 then add Wideband and txpwr evlp element*/
    if(ic->ic_chanchange_chwidth != CHWIDTH_VHT20) {
        if (ic->ic_wb_subelem) {
            frm = ieee80211_add_vht_wide_bw_switch(frm, ni, ic, subtype, 0);
        }

        frm = ieee80211_add_vht_txpwr_envlp(frm, ni, ic, subtype,
                                    IEEE80211_VHT_TXPWR_IS_SUB_ELEMENT);
    }
    /* If frame is filled with sub elements then add element id and len*/
    if((frm-2) != efrm)
    {
       ie_len = frm - efrm - 2;
       *efrm++ = IEEE80211_ELEMID_CHAN_SWITCH_WRAP;
       *efrm = ie_len;
       /* updating efrm with actual index*/
       efrm = frm;
    }
    return efrm;
}

u_int8_t *
ieee80211_add_timeout_ie(u_int8_t *frm, struct ieee80211_node *ni, size_t ie_len, u_int32_t tsecs)
{
    struct ieee80211_ie_timeout *lifetime = (struct ieee80211_ie_timeout *) frm;

    OS_MEMSET(frm, 0, ie_len);
    lifetime->ie_type = IEEE80211_ELEMID_TIMEOUT_INTERVAL;
    lifetime->ie_len = sizeof(struct ieee80211_ie_timeout) - 2;
    lifetime->interval_type = 3;
    lifetime->value = qdf_cpu_to_le32(tsecs);

    return frm + ie_len;
}

/**
 * @brief  Process power capability IE
 *
 * @param [in] ni  the STA that sent the IE
 * @param [in] ie  the IE to be processed
 */
void ieee80211_process_pwrcap_ie(struct ieee80211_node *ni, u_int8_t *ie)
{
    u_int8_t len;

    if (!ni || !ie) {
        return;
    }

    len = ie[1];
    if (len != 2) {
        IEEE80211_DISCARD_IE(ni->ni_vap,
            IEEE80211_MSG_ELEMID,
            "Power Cap IE", "invalid len %u", len);
        return;
    }

    ni->ni_min_txpower = ie[2];
    ni->ni_max_txpower = ie[3];
}

/**
 * @brief  Channels supported  IE
 *
 * @param [in] ni  the STA that sent the IE
 * @param [in] ie  the IE to be processed
 */
void ieee80211_process_supp_chan_ie(struct ieee80211_node *ni, u_int8_t *ie)
{
        struct ieee80211_ie_supp_channels *supp_chan = NULL;

        if (!ni || !ie) {
            return;
        }

        supp_chan = (struct ieee80211_ie_supp_channels *)ie;

        if (supp_chan->supp_chan_len != 2) {
            IEEE80211_DISCARD_IE(ni->ni_vap, IEEE80211_MSG_ELEMID,
                                 "802.11h channel supported IE",
                                 "invalid len %u", supp_chan->supp_chan_len);
            return;
        }

        ni->ni_first_channel = supp_chan->first_channel;
        ni->ni_nr_channels = supp_chan->nr_channels;
}

/*
 * IC ppett16 and ppet8 values corresponding to each ru
 * and nss are extracted and input to pack module
 */
void
he_ppet16_ppet8_pack(u_int8_t *he_ppet, u_int8_t *byte_idx_p, u_int8_t *bit_pos_used_p, u_int8_t ppet)
{
    int lft_sht, rgt_sht;
    int byte_idx = *byte_idx_p, bit_pos_used = *bit_pos_used_p;
    u_int8_t mask;

    if (bit_pos_used <= HE_PPET_MAX_BIT_POS_FIT_IN_BYTE) {
        lft_sht = bit_pos_used;
        he_ppet[byte_idx] |= (ppet << lft_sht);
        bit_pos_used += HE_PPET_FIELD;
        if (bit_pos_used == HE_PPET_BYTE) {
            bit_pos_used = 0;
            byte_idx++;
        }
    } else {
        lft_sht = bit_pos_used ;
        he_ppet[byte_idx] |= (ppet << lft_sht);
        bit_pos_used = 0;
        byte_idx++;
        rgt_sht = HE_PPET_BYTE - lft_sht;
        mask = (rgt_sht == 2) ? HE_PPET_RGT_ONE_BIT:
                                HE_PPET_RGT_TWO_BIT;
        he_ppet[byte_idx] |= ((ppet & mask ) >> rgt_sht);
        bit_pos_used = HE_PPET_FIELD - rgt_sht ;
    }

    *byte_idx_p = byte_idx;
    *bit_pos_used_p = bit_pos_used;
}

/*
 * IC ppett16 and ppet8 values corresponding to each ru
 * and nss are extracted and input to pack module
 */
void
he_ppet16_ppet8_extract_pack(u_int8_t *he_ppet, u_int8_t tot_nss,
                         u_int32_t ru_mask, u_int32_t *ppet16_ppet8) {

    u_int8_t ppet8_val, ppet16_val, byte_idx=0;
    u_int8_t bit_pos_used = HE_CAP_PPET_NSS_RU_BITS_FIXED;
    u_int8_t tot_ru = HE_PPET_TOT_RU_BITS;
    u_int8_t nss, ru;
    u_int32_t temp_ru_mask;

    for(nss=0; nss < tot_nss ; nss++) {    /* loop NSS */
        temp_ru_mask = ru_mask;
        for(ru=1; ru <= tot_ru ; ru++) {   /* loop RU */

            if(temp_ru_mask & 0x1) {

                /* extract ppet16 & ppet8 from IC he ppet handle */
                ppet16_val = HE_GET_PPET16(ppet16_ppet8, ru, nss);
                ppet8_val  = HE_GET_PPET8(ppet16_ppet8, ru, nss);

                /* pack ppet16 & ppet8 in contiguous byte araay*/
                he_ppet16_ppet8_pack(he_ppet, &byte_idx, &bit_pos_used, ppet16_val);
                he_ppet16_ppet8_pack(he_ppet, &byte_idx, &bit_pos_used, ppet8_val);
            }
            temp_ru_mask = temp_ru_mask >> 1;
        }
    }
}

void
hecap_ie_set(u_int8_t *hecap, u_int8_t idx, u_int8_t tot_bits,
                 u_int32_t value)  {

    u_int8_t fit_bits=0, byte_cnt=0, prev_fit_bits=0;
    idx = idx % 8;
    fit_bits = 8 - idx;
    fit_bits = (tot_bits > fit_bits) ? 8 - idx: tot_bits;

    while ((idx + tot_bits) > 8 ) {
        /* clear the target bit */
        hecap[byte_cnt] = hecap[byte_cnt] & ~(((1 << (fit_bits)) - 1) << (idx));
        hecap[byte_cnt] |= (((value >> prev_fit_bits) & ((1 << (fit_bits)) - 1)) << (idx));
        tot_bits = tot_bits - fit_bits;
        idx = idx + fit_bits;
        if( idx == 8 ) {
            idx =0;
            byte_cnt ++;
        }
        prev_fit_bits = prev_fit_bits + fit_bits;
        fit_bits = 8 - idx;
        fit_bits = ( tot_bits > fit_bits) ? 8 - idx: tot_bits ;
    }

    if ((idx + tot_bits) <= 8 ) {
        /* clear the target bit */
        hecap[byte_cnt] = hecap[byte_cnt] & ~(((1 << (tot_bits)) - 1) << (idx));
        hecap[byte_cnt] |= (((value >> prev_fit_bits) & ((1 << (tot_bits)) - 1)) << (idx));
    }
}

static void
ieee80211_set_hecap_rates(struct ieee80211com *ic,
                          struct ieee80211vap  *vap,
                          struct ieee80211_node *ni,
                          struct ieee80211_ie_hecap *hecap, bool enable_log)
{
    struct ieee80211_bwnss_map nssmap;
    uint8_t tx_chainmask  = ieee80211com_get_tx_chainmask(ic);
    uint8_t rx_streams    = ieee80211_get_rxstreams(ic, vap);
    uint8_t tx_streams    = ieee80211_get_txstreams(ic, vap);
    uint8_t nss           = MIN(rx_streams, tx_streams);
    uint8_t *nss_160      = &nssmap.bw_nss_160;
    uint8_t *hecap_txrx   = hecap->hecap_txrx;
    uint8_t unused_mcsnss_bytes = HE_UNUSED_MCSNSS_NBYTES;
    uint8_t chwidth;
    uint16_t rxmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
    uint16_t txmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);

    if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
       chwidth = vap->iv_chwidth;
    } else {
       chwidth = ic_cw_width;
    }

    /* Reset *nss_160 since the following fn
     * will not get called when ni_phymode
     * < IEEE80211_MODE_11AXA_HE160 and this
     * variable will hold garbage at that point
     */
    *nss_160 = 0;

    /* Get the nss vs chainmask mapping by
     * calling ic->ic_get_bw_nss_mapping.
     * This function returns nss for a given
     * chainmask which is nss = (1/2)nstreams
     * for HE currently (if ni_phymode is
     * IEEE80211_MODE_11AXA_HE160 or IEEE80211_
     * MODE_11AXA_HE80_80) where nstreams is
     * the number of streams for that chainmask.
     */
    if(chwidth >= IEEE80211_CWM_WIDTH160 &&
            ic->ic_get_bw_nss_mapping) {
        if(ic->ic_get_bw_nss_mapping(vap, &nssmap, tx_chainmask)) {
            /* if error then reset nssmap */
            *nss_160 = 0;
        }
    }

    /* get the intersected (user-set vs target caps)
     * values of mcsnssmap */
    ieee80211vap_get_insctd_mcsnssmap(vap, rxmcsnssmap, txmcsnssmap);

    switch(chwidth) {
        case IEEE80211_CWM_WIDTH160:
            if (vap->iv_des_mode == IEEE80211_MODE_11AXA_HE80_80 ||
                    IEEE80211_IS_CHAN_11AXA_HE80_80(ic->ic_curchan)) {
                /* mcsnssmap for bw80p80 */
                /* For > 80 MHz BW we will pack mcs_nss info for only the current
                 * value of nss which is half the no of streams for the current
                 * value of the chainmask - retrieved by ic->ic_get_bw_nss_mapping()
                 */
                (void)qdf_set_u16((uint8_t *)&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX8],
                    HE_GET_MCS_NSS_BITS_TO_PACK(rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
                        *nss_160));
                (void)qdf_set_u16((uint8_t *)&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX10],
                    HE_GET_MCS_NSS_BITS_TO_PACK(txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
                        *nss_160));

                HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX8],
                        *nss_160);
                HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX10],
                        *nss_160);

                unused_mcsnss_bytes -= HE_NBYTES_MCS_NSS_FIELD_PER_BAND;
            }

            /* mcsnssmap for bw160 */
            /* For > 80 MHz BW we will pack mcs_nss info for only the current
             * value of nss which is half the no of streams for the current
             * value of the chainmask - retrieved by ic->ic_get_bw_nss_mapping()
             */
            (void)qdf_set_u16((uint8_t *)&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX4],
                HE_GET_MCS_NSS_BITS_TO_PACK(rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
                    *nss_160));
            (void)qdf_set_u16((uint8_t *)&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX6],
                HE_GET_MCS_NSS_BITS_TO_PACK(txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
                    *nss_160));

            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX4],
                    *nss_160);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX6],
                    *nss_160);

            unused_mcsnss_bytes -= HE_NBYTES_MCS_NSS_FIELD_PER_BAND;

            /* fall through */
        default:
            /* mcsnssmap for bw<=80 */
            /* For <= 80 MHz BW we will pack mcs_nss info for only the current
             * value of nss
             */
            (void)qdf_set_u16((uint8_t *)&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX0],
                HE_GET_MCS_NSS_BITS_TO_PACK(rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
                     nss));
            (void)qdf_set_u16((uint8_t *)&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX2],
                HE_GET_MCS_NSS_BITS_TO_PACK(txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
                     nss));

            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX0],
                     nss);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX2],
                     nss);

            unused_mcsnss_bytes -= HE_NBYTES_MCS_NSS_FIELD_PER_BAND;
            break;
    }

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s hecap->hecap_txrx[0]=%x hecap->hecap_txrx[1]=%x"
        " hecap->hecap_txrx[2]=%x hecap->hecap_txrx[3]=%x"
        " hecap->hecap_txrx[4]=%x hecap->hecap_txrx[5]=%x\n"
        " hecap->hecap_txrx[6]=%x hecap->hecap_txrx[7]=%x"
        " hecap->hecap_txrx[8]=%x hecap->hecap_txrx[9]=%x"
        " hecap->hecap_txrx[10]=%x hecap->hecap_txrx[11]=%x"
        " nss=%x *nss_160=%x, chwidth=%x"
        " \n",__func__,
         hecap_txrx[HECAP_TXRX_MCS_NSS_IDX0], hecap_txrx[HECAP_TXRX_MCS_NSS_IDX1],
         hecap_txrx[HECAP_TXRX_MCS_NSS_IDX2], hecap_txrx[HECAP_TXRX_MCS_NSS_IDX3],
         hecap_txrx[HECAP_TXRX_MCS_NSS_IDX4], hecap_txrx[HECAP_TXRX_MCS_NSS_IDX5],
         hecap_txrx[HECAP_TXRX_MCS_NSS_IDX6], hecap_txrx[HECAP_TXRX_MCS_NSS_IDX7],
         hecap_txrx[HECAP_TXRX_MCS_NSS_IDX8], hecap_txrx[HECAP_TXRX_MCS_NSS_IDX9],
         hecap_txrx[HECAP_TXRX_MCS_NSS_IDX10], hecap_txrx[HECAP_TXRX_MCS_NSS_IDX11],
         nss, *nss_160, chwidth
         );
    }

    hecap->elem_len -= unused_mcsnss_bytes;
}

static void
hecap_override_channelwidth(struct ieee80211vap *vap,
                            u_int32_t *ch_width,
                            struct ieee80211_node *ni) {

    enum ieee80211_phymode des_mode = 0;
    u_int32_t width_mask = -1, band_width, width_val;
    u_int32_t ru_mask, ru_width;

    width_val = *ch_width;
    /* Use current mode for AP vaps and des_mode for Non-AP */
    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        des_mode = vap->iv_cur_mode;
    } else {
        des_mode = vap->iv_des_mode;
    }

    /* derive bandwidth mask */
    switch(des_mode)
    {
        case IEEE80211_MODE_11AXG_HE20:
        case IEEE80211_MODE_11AXA_HE20:
	    width_mask = IEEE80211_HECAP_PHY_CHWIDTH_11AX_HE20_MASK;
            break;
        case IEEE80211_MODE_11AXG_HE40PLUS:
        case IEEE80211_MODE_11AXG_HE40MINUS:
        case IEEE80211_MODE_11AXG_HE40:
	    width_mask = IEEE80211_HECAP_PHY_CHWIDTH_11AXG_HE40_MASK;
            break;
        case IEEE80211_MODE_11AXA_HE40PLUS:
        case IEEE80211_MODE_11AXA_HE40MINUS:
        case IEEE80211_MODE_11AXA_HE40:
	    width_mask = IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE40_MASK;
            break;
        case IEEE80211_MODE_11AXA_HE80:
	    width_mask = IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE40_HE80_MASK;
            break;
        case IEEE80211_MODE_11AXA_HE160:
	    width_mask = IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE40_HE80_HE160_MASK;
            break;
        case IEEE80211_MODE_11AXA_HE80_80:
	    width_mask = IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE40_HE80_HE160_HE80_80_MASK;
            break;
        default:
            break;
    }

    band_width = width_val & width_mask;

    if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
        /* derive ru mask */
        ru_mask = IEEE80211_IS_CHAN_11AXG(vap->iv_bsschan) ?
                  IEEE80211_HECAP_PHY_CHWIDTH_11AXG_RU_MASK:
                  IEEE80211_HECAP_PHY_CHWIDTH_11AXA_RU_MASK;

        ru_width = width_val & ru_mask;
    }

    /* set right phymode bandwidth as per des mode (current mode in ap) */
    width_val =(width_val & IEEE80211_HECAP_PHY_CHWIDTH_11AX_BW_ONLY_ZEROOUT_MASK)
                  | band_width;

    if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
        /* set right ru as per desired channel */
        width_val = ((width_val &
            IEEE80211_HECAP_PHY_CHWIDTH_11AX_RU_ONLY_ZEROOUT_MASK) | ru_width);
    } else {
        width_val = (width_val &
            IEEE80211_HECAP_PHY_CHWIDTH_11AX_RU_ONLY_ZEROOUT_MASK);
    }

    *ch_width = width_val;
}

u_int8_t *
ieee80211_add_hecap(u_int8_t *frm, struct ieee80211_node *ni,
                     struct ieee80211com *ic, u_int8_t subtype)
{
    struct ieee80211_ie_hecap *hecap  = (struct ieee80211_ie_hecap *)frm;
    struct ieee80211_he_handle *ic_he = &ic->ic_he;
    struct ieee80211vap  *vap         = ni->ni_vap;
    u_int32_t *ic_hecap_phy, val, ru_mask;
    u_int64_t ic_hecap_mac;
    u_int8_t *he_ppet, *hecap_phy_info, *hecap_mac_info;
    u_int8_t ppet_pad_bits, ppet_tot_bits, ppet_bytes;
    u_int8_t ppet_present, ru_set_count = 0;
    u_int8_t rx_streams = ieee80211_get_rxstreams(ic, vap);
    u_int8_t tx_streams = ieee80211_get_txstreams(ic, vap);
    u_int8_t nss        = MIN(rx_streams, tx_streams);
    bool enable_log     = false;
    int hecaplen;
    uint8_t chwidth;

    /* deduct the variable size fields before
     * memsetting hecap to 0
     */
    qdf_mem_zero(hecap,
            (sizeof(struct ieee80211_ie_hecap)
             - HECAP_TXRX_MCS_NSS_SIZE - HECAP_PPET16_PPET8_MAX_SIZE));

    hecap->elem_id     = IEEE80211_ELEMID_EXTN;
    /* elem id + len = 2 bytes  readjust based on
     *  mcs-nss and ppet fields
     */
    hecap->elem_len    = (sizeof(struct ieee80211_ie_hecap) -
                                         IEEE80211_IE_HDR_LEN);
    hecap->elem_id_ext = IEEE80211_ELEMID_EXT_HECAP;

    if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
        subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) {

        enable_log = true;
    }

    qdf_mem_copy(&ic_hecap_mac, ic_he->hecap_macinfo, sizeof(ic_he->hecap_macinfo));
    hecap_mac_info = &hecap->hecap_macinfo[0];

    /* Fill in default from IC HE MAC Capabilities
       only four bytes are copied from IE HE cap */
    qdf_mem_copy(&hecap->hecap_macinfo, &ic_he->hecap_macinfo,
                 sizeof(ic_he->hecap_macinfo));

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
          "%s IC hecap_mac_info = %x \n",__func__, ic_he->hecap_macinfo);
    }


    /* If MAC config override required for various MAC params,
      override MAC cap fields based on the vap HE MAC configs
      Each MAC values are taken from from IC and packed
      to HE MAC cap byte field
    */

    /* The user configured iv_he_ctrl value is intersected with
     * the target-cap for this field. Hence we can use the user
     * configured value here.
     */
    val = vap->iv_he_ctrl;
    HECAP_MAC_HECTRL_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Ctrl = %x \n",__func__, val, vap->iv_he_ctrl);
    }

    val = HECAP_MAC_TWTREQ_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_TWTREQ_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE TWT REQ = %x \n",__func__,
         val, vap->iv_he_twtreq);
    }

    /* the vap variable is initialized from the corresponding
     * field stored in ic at service_ready processing and can
     * be overwritten by user
     */
    val = vap->iv_twt_rsp;
    HECAP_MAC_TWTRSP_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE TWT RES = %x \n",__func__, val,
         vap->iv_he_twtres);
    }

    /* the user-configured he_frag value is intersected
     * with the target-cap for this field so that we
     * can directly use the user-configured value here.
     */
    val = vap->iv_he_frag;
    HECAP_MAC_HEFRAG_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Frag = %x \n",__func__, val, vap->iv_he_frag);
    }

#if SUPPORT_11AX_D3
    val = HECAP_MAC_MAXFRAGMSDUEXP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MAXFRAGMSDUEXP_SET_TO_IE(&hecap_mac_info, val);
#else
    val = HECAP_MAC_MAXFRAGMSDU_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MAXFRAGMSDU_SET_TO_IE(&hecap_mac_info, val);
#endif
    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Max Frag MSDU = %x \n",__func__,
         val, vap->iv_he_max_frag_msdu);
    }

    val = HECAP_MAC_MINFRAGSZ_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MINFRAGSZ_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE MIN Frag size = %x \n",__func__,
         val, vap->iv_he_min_frag_size);
    }

    /* According to 11ax spec D3.0 section 9.4.2.237
     * the MAC Padding Duration field should be
     * reserved for an AP.
     */
    if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
        val = HE_MAC_TRIGPADDUR_VALUE_RESERVED;
    } else {
        val = HECAP_MAC_TRIGPADDUR_GET_FROM_IC(ic_hecap_mac);
    }
    HECAP_MAC_TRIGPADDUR_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s IC val = %x Trigger Pad Dur \n",__func__, val);
    }

#if SUPPORT_11AX_D3
    val = HECAP_MAC_MTIDRXSUP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MTIDRXSUP_SET_TO_IE(&hecap_mac_info, val);
#else
    val = HECAP_MAC_MTID_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MTID_SET_TO_IE(&hecap_mac_info, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Multi Tid Aggr  = %x \n",
         __func__, val, vap->iv_he_multi_tid_aggr);
    }

    /* According to 11ax spec D2.0 section 9.4.2.237
     * this particular capability should be enabled
     * only if +HTC-HE Support is enabled.
     */
    if (vap->iv_he_ctrl) {
        val = HECAP_MAC_HELKAD_GET_FROM_IC(ic_hecap_mac);
    } else {
        val = 0;
    }
    HECAP_MAC_HELKAD_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Link Adapt  = %x \n",
         __func__, val, vap->iv_he_link_adapt);
    }

    val = HECAP_MAC_AACK_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_AACK_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE All Ack  = %x \n",
         __func__, val, vap->iv_he_all_ack);
    }

    /* According to 11ax spec D2.0/D3.0 section 9.4.2.237
     * this particular capability should be enabled
     * only if +HTC-HE Support is enabled. Moreover,
     * UMRS Rx is only expected in a STA. AP is not
     * UMRS RX capable.
     */
#if SUPPORT_11AX_D3
    if ((vap->iv_opmode == IEEE80211_M_STA) && vap->iv_he_ctrl) {
        val =  HECAP_MAC_TRSSUP_GET_FROM_IC(ic_hecap_mac);
    } else {
        val = 0;
    }
    HECAP_MAC_TRSSUP_SET_TO_IE(&hecap_mac_info, val);
#else
    if ((vap->iv_opmode == IEEE80211_M_STA) && vap->iv_he_ctrl) {
        val =  HECAP_MAC_ULMURSP_GET_FROM_IC(ic_hecap_mac);
    } else {
        val = 0;
    }
    HECAP_MAC_ULMURSP_SET_TO_IE(&hecap_mac_info, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE UL MU Resp Sced  = %x \n",
         __func__, val, vap->iv_he_ul_mu_sched);
    }

    /* According to 11ax spec D2.0 section 9.4.2.237
     * this particular capability should be enabled
     * only if +HTC-HE Support is enabled.
     */
    if ((vap->iv_he_ctrl) && (vap->iv_he_bsr_supp)){
        val = HECAP_MAC_BSR_GET_FROM_IC(ic_hecap_mac);
    } else {
        val = 0;
    }
    HECAP_MAC_BSR_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Actrl BSR = %x \n",
         __func__, val, vap->iv_he_actrl_bsr);
    }

    val = HECAP_MAC_BCSTTWT_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_BCSTTWT_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Bcast TWT  = %x \n",
         __func__, val, vap->iv_he_bcast_twt);
    }

    val = HECAP_MAC_32BITBA_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_32BITBA_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE 32 Bit BA  = %x \n",
         __func__, val, vap->iv_he_32bit_ba);
    }

    val = HECAP_MAC_MUCASCADE_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MUCASCADE_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE MU Cascade  = %x \n",
         __func__, val, vap->iv_he_mu_cascade);
    }

    val = HECAP_MAC_ACKMTIDAMPDU_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_ACKMTIDAMPDU_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x ACK Multi Tid Aggr\n",__func__, val);
    }

#if SUPPORT_11AX_D3
    /* According to 11ax spec doc D3.0 section 9.4.2.237.2 B24 of the
     * HE MAC cap is now a reserved bit */
    HECAP_MAC_RESERVED_SET_TO_IE(&hecap_mac_info, 0);
#else
    val = HECAP_MAC_GROUPMSTABA_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_GROUPMSTABA_SET_TO_IE(&hecap_mac_info, val);
#endif
    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x Group Addr Multi Sta BA DL MU \n",
         __func__, val);
    }

    /* OMI can only be sent as part of A-control in
     * HT Control field. So, +HTC-HE Support is mandatory
     * for OMI support.  Moreover, OMI RX is mandatory
     * for AP.
     */
    if (vap->iv_he_ctrl) {
        val = HECAP_MAC_OMI_GET_FROM_IC(ic_hecap_mac);
    } else {
        val = 0;
    }
    HECAP_MAC_OMI_SET_TO_IE(&hecap_mac_info, val);

    if (!val && (vap->iv_opmode == IEEE80211_M_HOSTAP)) {
        qdf_print("Mandatory AP MAC CAP OM Control is"
                  "being advertised as disabled");
    }

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE OMI = %x \n",
         __func__, val, vap->iv_he_omi);
    }

    val = HECAP_MAC_OFDMARA_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_OFDMARA_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE OMI = %x \n",
         __func__, val, vap->iv_he_ofdma_ra);
    }

#if SUPPORT_11AX_D3
    val = HECAP_MAC_MAXAMPDULEN_EXPEXT_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MAXAMPDULEN_EXPEXT_SET_TO_IE(&hecap_mac_info, val);
#else
    val = HECAP_MAC_MAXAMPDULEN_EXP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MAXAMPDULEN_EXP_SET_TO_IE(&hecap_mac_info, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x  Max AMPDU Len Exp \n",__func__, val);
    }

    val = HECAP_MAC_AMSDUFRAG_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_AMSDUFRAG_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE AMSDU Frag  = %x \n",
         __func__, val, vap->iv_he_amsdu_frag);
    }

    val = HECAP_MAC_FLEXTWT_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_FLEXTWT_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x VAP HE Flex TWT= %x \n",
         __func__, val, vap->iv_he_flex_twt);
    }

    val = HECAP_MAC_MBSS_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MBSS_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x Rx Ctrl to Multi BSS\n",__func__, val);
    }

    val = HECAP_MAC_BSRP_BQRP_AMPDU_AGGR_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_BSRP_BQRP_AMPDU_AGGR_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x BSR AMPDU \n",__func__, val);
    }

    val = HECAP_MAC_QTP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_QTP_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x QTP \n",__func__, val);
    }

    /* According to 11ax spec D2.0/D3.0 section 9.4.2.237
     * this particular capability should be enabled
     * only if +HTC-HE Support is enabled.
     */
    if (vap->iv_he_ctrl) {
        val = HECAP_MAC_ABQR_GET_FROM_IC(ic_hecap_mac);
    } else {
        val = 0;
    }
    HECAP_MAC_ABQR_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x Aggr BQR \n",__func__, val);
    }

#if SUPPORT_11AX_D3
    val = HECAP_MAC_SRPRESP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_SRPRESP_SET_TO_IE(&hecap_mac_info, val);
#else
    val = HECAP_MAC_SRRESP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_SRRESP_SET_TO_IE(&hecap_mac_info, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x SR Responder \n",__func__, val);
    }

    val = HECAP_MAC_NDPFDBKRPT_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_NDPFDBKRPT_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x NDK Feedback Report \n",__func__, val);
    }

    val = HECAP_MAC_OPS_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_OPS_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x OPS \n",__func__, val);
    }

    /* the vap variable is initialized from the corresponding
     * field stored in ic at service_ready processing and can
     * be overwritten by user
     */
    val = vap->iv_he_amsdu_in_ampdu_suprt;
    HECAP_MAC_AMSDUINAMPDU_SET_TO_IE(&hecap_mac_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x AMSDU in AMPDU \n",__func__, val);
    }

#if SUPPORT_11AX_D3
    val = HECAP_MAC_MTIDTXSUP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_MTIDTXSUP_SET_TO_IE(&hecap_mac_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x MTID AGGR TX Support \n",__func__, val);
    }

    val = HECAP_MAC_HESUBCHAN_TXSUP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_HESUBCHAN_TXSUP_SET_TO_IE(&hecap_mac_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x HE Sub-channel Selective TX Support \n",__func__, val);
    }

    /* According to 11ax spec D3.3 section 9.4.2.242.2
     * the UL 2x996-tone RU Support field should be
     * reserved for an AP.
     */
    if(vap->iv_opmode != IEEE80211_M_HOSTAP) {
        val = HECAP_MAC_UL2X996TONERU_GET_FROM_IC(ic_hecap_mac);
    }
    else {
        val = 0;
    }
    HECAP_MAC_UL2X996TONERU_SET_TO_IE(&hecap_mac_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x UL 2X996-tone RU Support \n",__func__, val);
    }

    val = HECAP_MAC_OMCTRLULMU_DISRX_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_OMCTRLULMU_DISRX_SET_TO_IE(&hecap_mac_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x OM control UL MU Disable RX Support \n",__func__, val);
    }

    /* According to 11ax spec D3.3 section 9.4.2.242.2
     * the Dynamic SM Power Save field should be
     * reserved for an AP.
     */
    if(vap->iv_opmode != IEEE80211_M_HOSTAP) {
        val = HECAP_MAC_DYNAMICSMPS_GET_FROM_IC(ic_hecap_mac);
    }
    else {
        val = 0;
    }
    HECAP_MAC_DYNAMICSMPS_SET_TO_IE(&hecap_mac_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x HE Dynamic SM Power Save \n", __func__, val);
    }

    val = HECAP_MAC_PUNCSOUNDSUPP_GET_FROM_IC(ic_hecap_mac);
    HECAP_MAC_PUNCSOUNDSUPP_SET_TO_IE(&hecap_mac_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x Punctured Sounding Support \n", __func__, val);
    }

    /* According to 11ax spec D3.3 section 9.4.2.242.2
     * the HT And VHT Trigger Frame Rx Support field should be
     * reserved for an AP.
     */
    if(vap->iv_opmode != IEEE80211_M_HOSTAP) {
        val = HECAP_MAC_HTVHT_TFRXSUPP_GET_FROM_IC(ic_hecap_mac);
    }
    else {
        val = 0;
    }
    HECAP_MAC_HTVHT_TFRXSUPP_SET_TO_IE(&hecap_mac_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s IC val = %x HT & VHT Trigger Frame Rx Support \n", __func__, val);
    }

    HECAP_MAC_RESERVED_SET_TO_IE(&hecap_mac_info, 0);
#else
    HECAP_MAC_RESERVED_SET_TO_IE(&hecap_mac_info, 0);
#endif

    /* Fill HE PHY capabilities */
    ic_hecap_phy = &ic_he->hecap_phyinfo[IC_HECAP_PHYDWORD_IDX0];

    hecap_phy_info = &hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX0];

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s IC hecap_phyinfo[0]=%x hecap_phyinfo[1]=%x hecap_phyinfo[2]=%x \n",
        __func__, ic_he->hecap_phyinfo[HECAP_PHYBYTE_IDX0],
        ic_he->hecap_phyinfo[HECAP_PHYBYTE_IDX1],
        ic_he->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }
#if SUPPORT_11AX_D3
    HECAP_PHY_RESERVED_SET_TO_IE(&hecap_phy_info, 0);
#else
    /* If PHY config override required for various phy params,
      override PHY cap fields based on the vap HE PHY configs
      Each PHy values are taken from from IC and packed
      to HE phy cap byte field
    */

    val = HECAP_PHY_DB_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_DB_SET_TO_IE(&hecap_phy_info, val);
#endif
    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
               "%s Dual Band Val=%x hecap->hecap_phyinfo[0]=%x \n",
                __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX0]);
    }

    val = HECAP_PHY_CBW_GET_FROM_IC(ic_hecap_phy);
    hecap_override_channelwidth(vap, &val, ni);
    HECAP_PHY_CBW_SET_TO_IE(&hecap_phy_info, val);
    /* save chwidth as the same is required as
     * a check for bfee_sts_gt80 cap
     */
    chwidth = val;

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
                "%s Channel Width Val=%x hecap->hecap_phyinfo[0]=%x \n",
                 __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX0]);
    }

    val = HECAP_PHY_PREAMBLEPUNCRX_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_PREAMBLEPUNCRX_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
              "%s RX Preamble Punc Val=%x hecap->hecap_phyinfo[1]=%x \n",
               __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

    val = HECAP_PHY_COD_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_COD_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
              "%s DCM Val=%x hecap->hecap_phyinfo[1]=%x \n",
               __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

    val = HECAP_PHY_LDPC_GET_FROM_IC(ic_hecap_phy);
    if (!(val && vap->vdev_mlme->proto.generic.ldpc)) {
        val = 0;
    }
    HECAP_PHY_LDPC_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
                    "%s LDPC Val=%x hecap->hecap_phyinfo[1]=%x \n",
                     __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

    /* the vap variable is initialized from the corresponding
     * field stored in ic at service_ready processing and can
     * be overwritten by user
     */
    val = vap->iv_he_1xltf_800ns_gi;
    HECAP_PHY_SU_1XLTFAND800NSECSGI_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s LTF & GI Val=%x hecap->hecap_phyinfo[1]=%x\n"
             ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

#if SUPPORT_11AX_D3
    val = HECAP_PHY_MIDAMBLETXRXMAXNSTS_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_MIDAMBLETXRXMAXNSTS_SET_TO_IE(&hecap_phy_info, val);
#else
    val = HECAP_PHY_MIDAMBLERXMAXNSTS_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_MIDAMBLERXMAXNSTS_SET_TO_IE(&hecap_phy_info, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s Midamble Rx Max NSTS Val=%x hecap->hecap_phyinfo[0]=%x"
            " hecap->hecap_phyinfo[1]=%x\n"
             ,__func__, val,
             hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX0],
             hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

    val = HECAP_PHY_LTFGIFORNDP_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_LTFGIFORNDP_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s LTF & GI NDP  Val=%x hecap->hecap_phyinfo[2]=%x\n"
          ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    if ((vap->iv_tx_stbc) && (tx_streams > 1)) {
        val = HECAP_PHY_TXSTBC_GET_FROM_IC(ic_hecap_phy);
    } else {
        val = 0;
    }
    HECAP_PHY_TXSTBC_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
       "%s TXSTBC LTEQ 80 Val=%x hecap->hecap_phyinfo[2]=%x \n"
        ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    if((vap->iv_rx_stbc) && (rx_streams > 0)) {
        val = HECAP_PHY_RXSTBC_GET_FROM_IC(ic_hecap_phy);
    } else {
        val = 0;
    }
    HECAP_PHY_RXSTBC_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
       "%s RXSTBC LTEQ 80 Val=%x hecap->hecap_phyinfo[2]=%x \n"
        ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_TXDOPPLER_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_TXDOPPLER_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s TX Doppler Val=%x hecap->hecap_phyinfo[2]=%x \n"
         ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_RXDOPPLER_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_RXDOPPLER_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s RX Doppler Val=%x hecap->hecap_phyinfo[2]=%x \n"
          ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    if (vap->iv_he_ul_mumimo) {
        val = HECAP_PHY_UL_MU_MIMO_GET_FROM_IC(ic_hecap_phy);
    } else {
        val = 0;
    }
    HECAP_PHY_UL_MU_MIMO_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s UL MU MIMO Val=%x hecap->hecap_phyinfo[2]=%x \n"
        ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    if (vap->iv_he_ul_muofdma) {
        val = HECAP_PHY_ULOFDMA_GET_FROM_IC(ic_hecap_phy);
    } else {
        val = 0;
    }
    HECAP_PHY_ULOFDMA_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s UL OFDMA Val=%x  hecap->hecap_phyinfo[2]=%x \n",
         __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_DCMTX_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_DCMTX_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s TX DCM Val=%x  hecap->hecap_phyinfo[3]=%x \n",
        __func__, val,  hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    val = HECAP_PHY_DCMRX_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_DCMRX_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s RX DCM Val=%x  hecap->hecap_phyinfo[3]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    val = HECAP_PHY_ULHEMU_PPDU_PAYLOAD_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_ULHEMU_PPDU_PAYLOAD_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
           "%s UL HE MU PPDU Val=%x hecap->hecap_phyinfo[3]=%x  \n"
               ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    if (vap->iv_he_su_bfer) {
        val = HECAP_PHY_SUBFMR_GET_FROM_IC(ic_hecap_phy);
    } else {
        val = 0;
    }
    HECAP_PHY_SUBFMR_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s SU BFMR Val=%x hecap->hecap_phyinfo[3]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    /* the user-configured he_subfee value is intersected
     * with the target-cap for this field so that we
     * can directly use the user-configured value here.
     */
    val = vap->iv_he_su_bfee;
    HECAP_PHY_SUBFME_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s SU BFEE Val=%x hecap->hecap_phyinfo[4]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX4]);
    }

    /* According to 11ax draft D2.0 section 9.4.2.237.3,
     * mu_bfer capability can be enabled only if su_bfer
     * is already enabled
     */
    if (vap->iv_he_su_bfer && vap->iv_he_mu_bfer) {
        val = HECAP_PHY_MUBFMR_GET_FROM_IC(ic_hecap_phy);
    }
    else {
        val = 0;
    }
    HECAP_PHY_MUBFMR_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s MU BFMR Val=%x hecap->hecap_phyinfo[4]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX4]);
    }

    if (vap->iv_he_su_bfee) {
        /* the vap variable is initialized from the corresponding
         * field stored in ic at service_ready processing and can
         * be overwritten by user
         */
        val = vap->iv_he_subfee_sts_lteq80;
    } else {
        /* According to 11ax draft 3.3 section 9.4.2.242.3,
         * HE subfee_sts_lteq80 field is 0 if subfee role is
         * not supported
         */
        val = 0;
    }
    HECAP_PHY_BFMENSTSLT80MHZ_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s BFME STS LT 80 Mhz Val=%x hecap->hecap_phyinfo[4]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX4]);
    }

    /* chwidht was calculated during B1-B7 (channel width set)
     * population. A value >=2 will indicate support of >=80MHz
     * BW support
     */
    if (vap->iv_he_su_bfee && chwidth >= 2) {
        /* the vap variable is initialized from the corresponding
         * field stored in ic at service_ready processing and can
         * be overwritten by user
         */
        val = vap->iv_he_subfee_sts_gt80;
    } else {
        /* According to 11ax draft 3.3 section 9.4.2.242.3,
         * HE subfee_sts_gt80 field is 0 if subfee role is
         * not supported or if the Channel Width Set field
         * does not indicate support for bandwidths greater
         * than 80
         */
        val = 0;
    }
    HECAP_PHY_BFMENSTSGT80MHZ_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s BFME STS GT 80 Mhz Val=%x hecap->hecap_phyinfo[5]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX5]);
    }

    if (vap->iv_he_su_bfer) {
        /* No of sounding dimension field should be based
         * on chainmask rather than nss. nss indicates
         * tx/rx caps. Whereas, sounding dimension can
         * be as many as the no. of chains present.
         */
        val = MIN(ieee80211_getstreams(ic, ic->ic_tx_chainmask) - 1,
                HECAP_PHY_NUMSOUNDLT80MHZ_GET_FROM_IC(ic_hecap_phy));
        HECAP_PHY_NUMSOUNDLT80MHZ_SET_TO_IE(&hecap_phy_info, val);
    } else {
        /* According to 11ax draft D3.0 section 9.4.2.237.3,
         * no_of_sound_dimnsn_lteq80 is reserved if SU Bfmer
         * field is 0
         */
        val = 0;
    }

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Noof Sound Dim LTEQ 80 Mhz Val=%x hecap->hecap_phyinfo[5]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX5]);
    }

    /* For an AP current mode will be same as the mode it was brought up in.
     * For a STA the current mode will initially be same as the desired mode
     * and on association to an AP, current mode will be updated as per the
     * connection.
     */
    if (vap->iv_he_su_bfer && vap->iv_cur_mode > IEEE80211_MODE_11AXA_HE80) {
        /* No of sounding dimension field should be based
         * on chainmask rather than nss. nss indicates
         * tx/rx caps. Whereas, sounding dimension can
         * be as many as the no. of chains present.
         */
        val = MIN(ieee80211_getstreams(ic, ic->ic_tx_chainmask) - 1,
               HECAP_PHY_NUMSOUNDGT80MHZ_GET_FROM_IC(ic_hecap_phy));
    } else {
        /* According to 11ax draft D3.0 section 9.4.2.237.3,
         * no_of_sound_dimnsn_gt80 is reserved if SU Bfmer
         * field is 0
         */
        val = 0;
    }
    HECAP_PHY_NUMSOUNDGT80MHZ_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Noof Sound Dim GT 80 Mhz Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_NG16SUFEEDBACKLT80_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_NG16SUFEEDBACKLT80_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Ng16 SU Feedback Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_NG16MUFEEDBACKGT80_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_NG16MUFEEDBACKGT80_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Ng16 MU Feeback Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_CODBK42SU_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_CODBK42SU_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s CB SZ 4_2 SU Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_CODBK75MU_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_CODBK75MU_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s CB SZ 7_5 MU Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_BFFEEDBACKTRIG_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_BFFEEDBACKTRIG_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s BF FB Trigg Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_HEERSU_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_HEERSU_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s HE ER SU PPDU Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_DLMUMIMOPARTIALBW_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_DLMUMIMOPARTIALBW_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s DL MUMIMO Par BW Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_PPETHRESPRESENT_GET_FROM_IC(ic_hecap_phy);
    ppet_present = val;
    HECAP_PHY_PPETHRESPRESENT_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s PPE Thresh present Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_SRPSPRESENT_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_SRPPRESENT_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s SRPS SR Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_PWRBOOSTAR_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_PWRBOOSTAR_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Power Boost AR Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    /* the vap variable is initialized from the corresponding
     * field stored in ic at service_ready processing and can
     * be overwritten by user
     */
    val = vap->iv_he_4xltf_800ns_gi;
    HECAP_PHY_4XLTFAND800NSECSGI_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s 4X HE-LTF & 0.8 GI HE PPDU Val=%x hecap->hecap_phyinfo[7]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    /* the vap variable is initialized from the corresponding
     * field stored in ic at service_ready processing and can
     * be overwritten by user
     */
    val = vap->iv_he_max_nc;
    HECAP_PHY_MAX_NC_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s MAX Nc=%x hecap->hecap_phyinfo[7]=%x \n", __func__,
        val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_STBCTXGT80_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_STBCTXGT80_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s STBC Tx GT 80MHz=%x hecap->hecap_phyinfo[7]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_STBCRXGT80_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_STBCRXGT80_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s STBC Rx GT 80MHz=%x hecap->hecap_phyinfo[7]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_ERSU_4XLTF800NSGI_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_ERSU_4XLTF800NSGI_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s ERSU 4x LTF 800 ns GI=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_HEPPDU20IN40MHZ2G_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_HEPPDU20IN40MHZ2G_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s HE PPDU 20 in 40 MHZ 2G=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_HEPPDU20IN160OR80P80MHZ_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_HEPPDU20IN160OR80P80MHZ_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s HE PPDU 20 in 160 or 80+80 MHZ=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_HEPPDU80IN160OR80P80MHZ_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_HEPPDU80IN160OR80P80MHZ_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s HE PPDU 80 in 160 or 80+80 MHZ=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_ERSU1XLTF800NSGI_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_ERSU1XLTF800NSGI_SET_TO_IE(&hecap_phy_info, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s ERSU 1x LTF 800 ns GI=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

#if SUPPORT_11AX_D3
    val = HECAP_PHY_MIDAMBLETXRX2XAND1XLTF_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_MIDAMBLETXRX2XAND1XLTF_SET_TO_IE(&hecap_phy_info, val);
#else
    val = HECAP_PHY_MIDAMBLERX2XAND1XLTF_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_MIDAMBLERX2XAND1XLTF_SET_TO_IE(&hecap_phy_info, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Midamble Rx 2x and 1x LTF=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

#if SUPPORT_11AX_D3

    val = HECAP_PHY_DCMMAXBW_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_DCMMAXBW_SET_TO_IE(&hecap_phy_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s DCM Max BW=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_LT16HESIGBOFDMSYM_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_LT16HESIGBOFDMSYM_SET_TO_IE(&hecap_phy_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Longer Than 16 HE SIG-B OFDM Symbols Support=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_NONTRIGCQIFDBK_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_NONTRIGCQIFDBK_SET_TO_IE(&hecap_phy_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Non- Triggered CQI Feedback=%x hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    if (vap->iv_opmode == IEEE80211_M_STA) {
        val =  HECAP_PHY_TX1024QAMLT242TONERU_GET_FROM_IC(ic_hecap_phy);
    } else {
        val = 0;
    }
    HECAP_PHY_TX1024QAMLT242TONERU_SET_TO_IE(&hecap_phy_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Tx 1024- QAM < 242-tone RU Support=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_RX1024QAMLT242TONERU_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_RX1024QAMLT242TONERU_SET_TO_IE(&hecap_phy_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Rx 1024- QAM < 242-tone RU Support=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_RXFULLBWSUHEMUPPDU_COMPSIGB_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_RXFULLBWSUHEMUPPDU_COMPSIGB_SET_TO_IE(&hecap_phy_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Rx Full BW SU Using HE MU PPDU With Compressed SIGB=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_RXFULLBWSUHEMUPPDU_NONCOMPSIGB_GET_FROM_IC(ic_hecap_phy);
    HECAP_PHY_RXFULLBWSUHEMUPPDU_NONCOMPSIGB_SET_TO_IE(&hecap_phy_info, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Rx Full BW SU Using HE MU PPDU With Non-Compressed SIGB=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s IC HE PPET ru3_ru0[0]=%x ru3_ru0[1]=%x ru3_ru0[2]=%x"
        " ru3_ru0[3]=%x ru3_ru0[4]=%x ru3_ru0[5]=%x ru3_ru0[6]=%x"
        " ru3_ru0[7]=%x \n",__func__,
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[0],
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[1],
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[2],
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[3],
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[4],
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[5],
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[6],
         ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0[7]);
    }

     /* Fill in TxRx HE NSS & MCS support */
    ieee80211_set_hecap_rates(ic, vap, ni, hecap, enable_log);

    if(ppet_present) {

        /* Fill in default PPET Fields
           3 bits for no of SS + 4 bits for RU bit enabled
           count + no of SS * ru bit enabled count * 6 bits
           (3 bits for ppet16 and 3 bits for ppet8)
        */

        ru_mask = ic->ic_he.hecap_ppet.ru_mask;

        HE_GET_RU_BIT_SET_COUNT_FROM_RU_MASK(ru_mask, ru_set_count);

        ppet_tot_bits = HE_CAP_PPET_NSS_RU_BITS_FIXED +
                       (nss * ru_set_count * HE_TOT_BITS_PPET16_PPET8);

        ppet_pad_bits = HE_PPET_BYTE - (ppet_tot_bits % HE_PPET_BYTE);

        ppet_bytes = (ppet_tot_bits + ppet_pad_bits) / HE_PPET_BYTE;

        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s PET TOT Bits =%d PPET PAD Bits=%d PPET Bytes=%d \n", __func__,
            ppet_tot_bits, ppet_pad_bits, ppet_bytes);
        }

        if(ppet_bytes != HECAP_PPET16_PPET8_MAX_SIZE)  {
            /* readjusting length field as per ppet info */
            hecap->elem_len -= HECAP_PPET16_PPET8_MAX_SIZE - ppet_bytes;
        }

        /* mcs_nss is a variable field. Readjusting he_ppet
         * pointer according to mcs_nss field
         */
        he_ppet = ((uint8_t *) hecap + (hecap->elem_len -
                                 ppet_bytes + IEEE80211_IE_HDR_LEN));

        qdf_mem_zero(he_ppet, ppet_bytes);

        /* Fill no of SS*/
        he_ppet[0] = (nss-1) & IEEE80211_HE_PPET_NUM_SS;

        /* Fill RU Bit mask */
        he_ppet[0] |= (ic->ic_he.hecap_ppet.ru_mask << IEEE80211_HE_PPET_RU_COUNT_S);

        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
              "%s TOT NSS =%d  ru_mask=%x he_ppet[0]=%x \n",
               __func__, nss ,ic->ic_he.hecap_ppet.ru_mask, he_ppet[0]);
        }

        /* extract and pack PPET16 & PPET8 for each RU and for each NSS */
        he_ppet16_ppet8_extract_pack(he_ppet, nss,
                ic->ic_he.hecap_ppet.ru_mask,
                ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0);
    }

    /* elem id + len = 2 bytes */
    hecaplen = hecap->elem_len + IEEE80211_IE_HDR_LEN;

    if (enable_log)  {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
          "%s HE CAP Length=%d Hex length =%x \n",
           __func__, hecaplen, hecaplen);
    }

    return frm + hecaplen;
}

/*
 * NSS, RU , ppett16 and ppet8 are extracted from IE
 * and stored in corresponding ni HE structure
 */
void
he_ppet16_ppet8_parse( u_int32_t* out_ppet, u_int8_t* in_ppet) {

    uint8_t tot_nss, tot_ppet, mask1, mask2;
    uint8_t  ru_mask8, ru_mask16, ru_mask;
    int32_t tmp_ppet1, tmp_ppet2, ppet, ru_set_count=0;
    int byte_idx, start, nss, ru, bits_parsed;
    uint8_t  ppet8_idx=0, ppet16_idx=0;

    tot_nss = (in_ppet[0] & IEEE80211_HE_PPET_NUM_SS);
    ru_mask = ((in_ppet[0] & IEEE80211_HE_PPET_RU_MASK) >>
                 IEEE80211_HE_PPET_RU_COUNT_S) + 1;

    ru_mask8 = ru_mask16 = ru_mask;
    HE_GET_RU_BIT_SET_COUNT_FROM_RU_MASK(ru_mask, ru_set_count);

    /* 3 bits for no of SS + 4 bits for RU mask */
    bits_parsed = HE_CAP_PPET_NSS_RU_BITS_FIXED;
    tot_ppet = ru_set_count * HE_PPET16_PPET8;
    for (nss = 0; nss <= tot_nss; nss++) {
        for (ru = 1; ru <= tot_ppet; ru++) {
            start = bits_parsed + (nss * (tot_ppet * HE_PPET_FIELD)) +
                                                 (ru - 1) * HE_PPET_FIELD;
            byte_idx = start / HE_PPET_BYTE;
            start = start % HE_PPET_BYTE;

            mask1 = HE_PPET16_PPET8_MASK << start;
            if (start <= HE_PPET_MAX_BIT_POS_FIT_IN_BYTE) {
                /* parse ppet with in a byte*/
                ppet = (in_ppet[byte_idx] & mask1) >> start;
            } else {
                /* parse ppet in more than 1 byte*/
                tmp_ppet1 = (in_ppet[byte_idx] & mask1) >> start;
                mask2 = HE_PPET16_PPET8_MASK >> (HE_PPET_BYTE - start);
                tmp_ppet2 = (in_ppet[byte_idx + 1] & mask2) << (HE_PPET_BYTE - start);
                ppet = tmp_ppet1 | tmp_ppet2;
            }

            /* store in ni ppet field */
            if (ru % HE_PPET16_PPET8 == 1) {
                HE_NEXT_IDX_FROM_RU_MASK_AND_OLD_IDX(ru_mask8, ppet8_idx);
                HE_SET_PPET8(out_ppet, ppet,  ppet8_idx, nss);
            } else {
                HE_NEXT_IDX_FROM_RU_MASK_AND_OLD_IDX(ru_mask16, ppet16_idx);
                HE_SET_PPET16(out_ppet, ppet, ppet16_idx,  nss);
            }
        }
    }
}

/*
 * Phy capabilities values are extracted from IE
 * byte array based on idx & total bits
 */
u_int32_t
hecap_ie_get(u_int8_t *hecap, u_int8_t idx, u_int32_t
                tot_bits)  {

    u_int8_t fit_bits=0, byte_cnt=0, temp_val;
    u_int8_t cur_idx=0;
    u_int32_t val=0;

    temp_val = *(hecap);
    idx = idx % 8;
    fit_bits = 8 - idx;
    fit_bits = ( tot_bits > fit_bits) ? 8 - idx: tot_bits ;

    while (( tot_bits + idx) > 8 ) {
        val |= ((temp_val >> idx ) & ((1 << (fit_bits)) - 1)) << cur_idx;
        tot_bits = tot_bits - fit_bits;
        idx = idx + fit_bits;
        if( idx == 8 ) {
            idx = 0;
            byte_cnt ++;
            temp_val = *(hecap + byte_cnt);
        }
        cur_idx = cur_idx + fit_bits;

       fit_bits = 8 - idx;
       fit_bits = ( tot_bits > fit_bits) ? 8 - idx: tot_bits ;
    }

    if ((idx + tot_bits) <= 8 ) {
        val |= ((temp_val >> idx)  & ((1 << fit_bits) -1)) << cur_idx;
    }

    return val;
}

static bool
ieee80211_is_basic_mcsnss_requirement_met(uint16_t mcsnssmap,
                                          uint8_t basic_mcs,
                                          uint8_t basic_nss) {
    uint8_t peer_nss, peer_mcs, nss_count = 0;
    bool basic_mcsnss_req_met = true;

    HE_DERIVE_PEER_NSS_FROM_MCSMAP(mcsnssmap, peer_nss);

    /* if nss value derived from mcsnssmap is less
     * then the basic requirement of the bss then fail
     */
    if (peer_nss < basic_nss) {
        basic_mcsnss_req_met = false;
    }

    /* if mcs value for each of the streams that the peer
     * supports does not meet the basic requirement for
     * the bss then fail
     */
    for (nss_count = 0; ((nss_count < peer_nss) &&
        basic_mcsnss_req_met); nss_count++) {
        peer_mcs = mcsbits(mcsnssmap, nss_count+1);
        if ((peer_mcs == HE_INVALID_MCSNSSMAP) ||
              (peer_mcs < basic_mcs)) {
            basic_mcsnss_req_met = false;
        }
    }

    return basic_mcsnss_req_met;
}

static inline bool
ieee80211_is_basic_txrx_mcsnss_requirement_met(struct ieee80211_node *ni,
                                               uint8_t mcsnss_idx) {
    struct ieee80211_he_handle *ni_he = &ni->ni_he;
    struct ieee80211vap *vap          = ni->ni_vap;

    return (ieee80211_is_basic_mcsnss_requirement_met(
                ni_he->hecap_txmcsnssmap[mcsnss_idx],
                vap->iv_he_basic_mcs_for_bss, vap->iv_he_basic_nss_for_bss)
           &&
           ieee80211_is_basic_mcsnss_requirement_met(
                ni_he->hecap_rxmcsnssmap[mcsnss_idx],
                vap->iv_he_basic_mcs_for_bss, vap->iv_he_basic_nss_for_bss));
}

static void
ieee80211_hecap_parse_mcs_nss(struct ieee80211_node *ni,
                              struct ieee80211vap  *vap,
                              uint8_t *hecap_txrx,
                              bool enable_log,
                              uint8_t *mcsnssbytes) {
    struct ieee80211_bwnss_map nssmap;
    struct ieee80211_he_handle *ni_he = &ni->ni_he;
    struct ieee80211com *ic           = ni->ni_ic;
    uint8_t tx_chainmask              = ieee80211com_get_tx_chainmask(ic);
    uint8_t nss                       = MIN(ieee80211_get_rxstreams(ic, vap),
                                            ieee80211_get_txstreams(ic, vap));
    uint8_t *nss_160                  = &nssmap.bw_nss_160;
    uint16_t temp_self_mcsnssmap;
    uint16_t temp_peer_mcsnssmap;
    uint16_t rxmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
    uint16_t txmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
    bool  he_basic_mcsnss_req_met = false;
    uint32_t ni_bw80p80_nss, ni_bw160_nss, ni_streams;
    uint8_t chwidth;

    if(ni->ni_phymode > IEEE80211_MODE_11AXA_HE80_80) {
        ieee80211_note(vap, IEEE80211_MSG_HE,
              "%s WARNING!!! Unsupported ni_phymode=%x\n",
                      __func__, ni->ni_phymode);
        return;
    }

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
          "%s ni->ni_phymode is = 0x%x \n",__func__, ni->ni_phymode);
    }

    if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
        chwidth = vap->iv_chwidth;
    } else {
        chwidth = ic->ic_cwm_get_width(ic);
    }

    *nss_160 = 0;

    if(chwidth >= IEEE80211_CWM_WIDTH160 &&
            ic->ic_get_bw_nss_mapping) {
        if(ic->ic_get_bw_nss_mapping(vap, &nssmap, tx_chainmask)) {
            /* if error then reset nssmap */
            *nss_160 = 0;
        }
    }

    /* get the intersected (user-set vs target caps)
     * values of mcsnssmap */
    ieee80211vap_get_insctd_mcsnssmap(vap, rxmcsnssmap, txmcsnssmap);

    *mcsnssbytes = 0;
    switch(ni->ni_phymode) {
        case IEEE80211_MODE_11AXA_HE80_80:
            (void)qdf_get_u16(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX8],
                        &ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]);
            (void)qdf_get_u16(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX10],
                        &ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]);

            /* Set the bits for unsupported SS in the self RX map to Invalid */
            (void)qdf_get_u16((uint8_t *)&rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
                              &temp_self_mcsnssmap);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(
                    (uint8_t *)&temp_self_mcsnssmap, *nss_160);
            temp_peer_mcsnssmap = ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80];

            ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                INTERSECT_11AX_MCSNSS_MAP(temp_self_mcsnssmap, temp_peer_mcsnssmap);

            /* Set the bits for unsupported SS in the self TX map to Invalid */
            (void)qdf_get_u16((uint8_t *)&txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
                        &temp_self_mcsnssmap);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(
                    (uint8_t *)&temp_self_mcsnssmap, *nss_160);
            temp_peer_mcsnssmap = ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80];

            ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                INTERSECT_11AX_MCSNSS_MAP(temp_self_mcsnssmap, temp_peer_mcsnssmap);

            /* If mcsnssmap advertised by the peer does not meet
             * the basic mcsnss requirement as advertised in heop
             * then allow association only in VHT mode.
             */
            if (ieee80211_is_basic_txrx_mcsnss_requirement_met(
                        ni, HECAP_TXRX_MCS_NSS_IDX_80_80)) {
                HE_DERIVE_PEER_NSS_FROM_MCSMAP(
                    ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
                    ni_bw80p80_nss);
                ni->ni_bw80p80_nss = QDF_MAX(ni->ni_bw80p80_nss, ni_bw80p80_nss);
                he_basic_mcsnss_req_met = true;
            } else {
                ni->ni_phymode = IEEE80211_MODE_11AXA_HE160;
            }

            *mcsnssbytes += HE_NBYTES_MCS_NSS_FIELD_PER_BAND;

            /* fall through */
        case IEEE80211_MODE_11AXA_HE160:
            (void)qdf_get_u16(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX4],
                        &ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]);
            (void)qdf_get_u16(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX6],
                        &ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]);

            /* Set the bits for unsupported SS in the self RX map to Invalid */
            (void)qdf_get_u16((uint8_t *)&rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
                              &temp_self_mcsnssmap);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(
                    (uint8_t *)&temp_self_mcsnssmap, *nss_160);
            temp_peer_mcsnssmap = ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160];

            ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] =
                INTERSECT_11AX_MCSNSS_MAP(temp_self_mcsnssmap, temp_peer_mcsnssmap);

            /* Set the bits for unsupported SS in the self TX map to Invalid */
            (void)qdf_get_u16((uint8_t *)&txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
                              &temp_self_mcsnssmap);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(
                    (uint8_t *)&temp_self_mcsnssmap, *nss_160);
            temp_peer_mcsnssmap = ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160];

            ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] =
                INTERSECT_11AX_MCSNSS_MAP(temp_self_mcsnssmap, temp_peer_mcsnssmap);

            /* If mcsnssmap advertised by the peer does not meet
             * the basic mcsnss requirement as advertised in heop
             * then allow association only in VHT mode.
             */
            if (ieee80211_is_basic_txrx_mcsnss_requirement_met(
                        ni, HECAP_TXRX_MCS_NSS_IDX_160)) {
                HE_DERIVE_PEER_NSS_FROM_MCSMAP(
                    ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
                    ni_bw160_nss);
                ni->ni_bw160_nss = QDF_MAX(ni->ni_bw160_nss, ni_bw160_nss);
                he_basic_mcsnss_req_met = true;
            } else {
                ni->ni_phymode = IEEE80211_MODE_11AXA_HE80;
            }

            *mcsnssbytes += HE_NBYTES_MCS_NSS_FIELD_PER_BAND;

            /* fall through */
        default:
            (void)qdf_get_u16(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX0],
                        &ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]);
            (void)qdf_get_u16(&hecap_txrx[HECAP_TXRX_MCS_NSS_IDX2],
                        &ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]);

            /* Set the bits for unsupported SS in the self RX map to Invalid */
            (void)qdf_get_u16((uint8_t *)&rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
                              &temp_self_mcsnssmap);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(
                    (uint8_t *)&temp_self_mcsnssmap, nss);
            temp_peer_mcsnssmap = ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80];

            ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] =
                INTERSECT_11AX_MCSNSS_MAP(temp_self_mcsnssmap, temp_peer_mcsnssmap);

            /* Set the bits for unsupported SS in the self TX map to Invalid */
            (void)qdf_get_u16((uint8_t *)&txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
                              &temp_self_mcsnssmap);
            HE_RESET_MCS_VALUES_FOR_UNSUPPORTED_SS(
                    (uint8_t *)&temp_self_mcsnssmap, nss);
            temp_peer_mcsnssmap = ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80];

            ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] =
                INTERSECT_11AX_MCSNSS_MAP(temp_self_mcsnssmap, temp_peer_mcsnssmap);

            /* If mcsnssmap advertised by the peer does not meet
             * the basic mcsnss requirement as advertised in heop
             * then allow association only in VHT mode.
             */
            if (ieee80211_is_basic_txrx_mcsnss_requirement_met(
                        ni, HECAP_TXRX_MCS_NSS_IDX_80)) {
                HE_DERIVE_PEER_NSS_FROM_MCSMAP(
                    ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
                    ni_streams);
                ni->ni_streams = QDF_MAX(ni->ni_streams, ni_streams);
                he_basic_mcsnss_req_met = true;
            }

            *mcsnssbytes += HE_NBYTES_MCS_NSS_FIELD_PER_BAND;
            break;
    }

    if(!he_basic_mcsnss_req_met) {
        qdf_err("%s peer HE mcsnssmap does not meet basic requirenment"
                  " for the bss. Allowing association only in VHT mode", __func__);
        ni->ni_ext_flags &= ~IEEE80211_NODE_HE;
        qdf_mem_zero(&ni->ni_he, sizeof(struct ieee80211_he_handle));
    }

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s ni_he->rxmcsnssmap[80MHz]=%x ni_he->txmcsnssmap[80MHz]=%x"
            " ni_he->rxmcsnssmap[160MHz]=%x ni_he->txmcsnssmap[160MHz]=%x"
            " ni_he->rxmcsnssmap[80_80MHz]=%x ni_he->txmcsnssmap[80_80MHz]=%x"
            " *mcsnssbytes=%x \n",__func__,
             ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
             ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80],
             ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
             ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160],
             ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
             ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80],
             *mcsnssbytes
             );
    }
}

static void
hecap_parse_channelwidth(struct ieee80211_node *ni,
                         u_int32_t *in_width) {

    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    u_int32_t he_width = *in_width;
    u_int8_t chwidth, width_set;

    if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
        chwidth = vap->iv_chwidth;
    } else {
        chwidth = ic->ic_cwm_get_width(ic);
    }

    /* 11AX TODO (Phase II) . Width parsing needs to be
       revisited for addressing grey areas in spec
     */
    switch(chwidth) {
        case IEEE80211_CWM_WIDTH20:
            ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            break;

        case IEEE80211_CWM_WIDTH40:
            /* HTCAP Channelwidth will be set to max for 11ax */
            if (!(ni->ni_htcap & IEEE80211_HTCAP_C_CHWIDTH40)) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            } else {
                /* width_set check not required */
                ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            }
            break;

        case IEEE80211_CWM_WIDTH80:
            width_set = he_width & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE40_HE80_MASK;
            if (!(ni->ni_htcap & IEEE80211_HTCAP_C_CHWIDTH40)) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            } else if (!(ni->ni_vhtcap)) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            } else if(width_set) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
            } else {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            }
            break;

        case IEEE80211_CWM_WIDTH160:
            width_set = he_width & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE40_HE80_HE160_HE80_80_MASK;
            if (!(ni->ni_htcap & IEEE80211_HTCAP_C_CHWIDTH40)) {
                 ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            } else if (!(ni->ni_vhtcap)) {
                 ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            } else if(width_set & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE160) {
                 ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
            } else if(width_set & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE80_80) {
                 ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
            } else if(width_set & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE80) {
                 ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
            } else {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            }
            break;

        case IEEE80211_CWM_WIDTH80_80:
            width_set = he_width & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE40_HE80_HE160_HE80_80_MASK;
            if (!(ni->ni_htcap & IEEE80211_HTCAP_C_CHWIDTH40)) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            } else if (!(ni->ni_vhtcap)) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            } else if(width_set & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE80_80) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH80_80;
            } else if(width_set & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE160) {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
            } else if(width_set & IEEE80211_HECAP_PHY_CHWIDTH_11AXA_HE80) {
                   ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
            } else {
                ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            }
            break;

        default:
            /* Do nothing */
        break;
    }
}


void
ieee80211_parse_hecap(struct ieee80211_node *ni, u_int8_t *ie, u_int8_t subtype)
{
    struct ieee80211_ie_hecap *hecap  = (struct ieee80211_ie_hecap *)ie;
    struct ieee80211_he_handle *ni_he = &ni->ni_he;
    struct ieee80211com *ic           = ni->ni_ic;
    struct ieee80211vap *vap          = ni->ni_vap;
    uint32_t *ni_ppet, val=0, *ni_hecap_phyinfo;
    uint32_t ni_hecap_macinfo;
    uint8_t *hecap_phy_ie, ppet_present, *hecap_mac_ie;
    uint8_t rx_streams = ieee80211_get_rxstreams(ic, vap);
    uint8_t tx_streams = ieee80211_get_txstreams(ic, vap);
    uint8_t mcsnssbytes;
    bool enable_log = false;
    uint32_t ampdu_len = 0;


    /* Negotiated HE PHY Capability.
     * Parse & set to node HE handle
     */

    hecap_phy_ie = &hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX0];

    ni_hecap_phyinfo = &ni_he->hecap_phyinfo[HECAP_PHYBYTE_IDX0];

    /* Fill in default from IE HE MAC Capabilities */
    qdf_mem_copy(&ni_he->hecap_macinfo, &hecap->hecap_macinfo,
                 sizeof(ni_he->hecap_macinfo));

    qdf_mem_copy(&ni_hecap_macinfo, &ni_he->hecap_macinfo[0],
                 sizeof(ni_hecap_macinfo));

    if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
         subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) {

         enable_log = true;
    }

    /* Mark HE node */
    ni->ni_ext_flags |= IEEE80211_NODE_HE;

    hecap_mac_ie = &hecap->hecap_macinfo[HECAP_MACBYTE_IDX0];
#if SUPPORT_11AX_D3
    ampdu_len = HECAP_MAC_MAXAMPDULEN_EXPEXT_GET_FROM_IE(&hecap_mac_ie);
#else
    ampdu_len = HECAP_MAC_MAXAMPDULEN_EXP_GET_FROM_IE(&hecap_mac_ie);
#endif

    /* As per section 26.6.1 11ax Draft4.0, if the Max AMPDU Exponent Extension
     * in HE cap is zero, use the ni_maxampdu as calculated while parsing
     * VHT caps(if VHT caps is present) or HT caps (if VHT caps is not present).
     *
     * For non-zero value of Max AMPDU Extponent Extension in HE MAC caps,
     * if a HE STA sends VHT cap and HE cap IE in assoc request then, use
     * MAX_AMPDU_LEN_FACTOR as 20 to calculate max_ampdu length.
     * If a HE STA that does not send VHT cap, but HE and HT cap in assoc
     * request, then use MAX_AMPDU_LEN_FACTOR as 16 to calculate max_ampdu
     * length.
     */
    if(ampdu_len) {
        if (ni->ni_vhtcap) {
            ni->ni_maxampdu = (1u << (IEEE80211_HE_VHT_CAP_MAX_AMPDU_LEN_FACTOR + ampdu_len)) -1;
        } else if (ni->ni_htcap) {
            ni->ni_maxampdu = (1u << (IEEE80211_HE_HT_CAP_MAX_AMPDU_LEN_FACTOR + ampdu_len)) -1;
        }
    }

    val = HECAP_MAC_TWTREQ_GET_FROM_IE(&hecap_mac_ie);
    if (val)
        ni->ni_ext_flags |= IEEE80211_NODE_TWT_REQUESTER;

    val = HECAP_MAC_TWTRSP_GET_FROM_IE(&hecap_mac_ie);
    if (val)
        ni->ni_ext_flags |= IEEE80211_NODE_TWT_RESPONDER;

   /* Overriding Mac Capabilities based on vap configuration */


    /* Fill in default from IE HE PHY Capabilities */
#if !SUPPORT_11AX_D3
    val = HECAP_PHY_DB_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_DB_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
               "%s NI Dual Band Val=%x hecap->hecap_phyinfo[0]=%x \n",
                __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX0]);
    }
#endif

    val = HECAP_PHY_CBW_GET_FROM_IE(&hecap_phy_ie);
    hecap_parse_channelwidth(ni, &val);
    HECAP_PHY_CBW_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
          "%s NI Channel Width Val=%x ni_widhth= %x"
          "hecap->hecap_phyinfo[0]=%x \n",
           __func__, val, ni->ni_chwidth,
           hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX0]);
    }

    val = HECAP_PHY_PREAMBLEPUNCRX_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_PREAMBLEPUNCRX_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
              "%s NI RX Preamble Punc Val=%x hecap->hecap_phyinfo[1]=%x \n",
               __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

    val = HECAP_PHY_COD_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_COD_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
              "%s NI DCM Val=%x hecap->hecap_phyinfo[1]=%x \n",
               __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

    val = HECAP_PHY_LDPC_GET_FROM_IE(&hecap_phy_ie);
    if (!(val && vap->vdev_mlme->proto.generic.ldpc)) {
        val = 0;
    }
    HECAP_PHY_LDPC_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
                    "%s NI LDPC Val=%x hecap->hecap_phyinfo[1]=%x \n",
                     __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

    val = HECAP_PHY_SU_1XLTFAND800NSECSGI_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_SU_1XLTFAND800NSECSGI_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s NI LTF & GI Val=%x hecap->hecap_phyinfo[1]=%x \n"
            ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1]);
    }

#if SUPPORT_11AX_D3
    val = HECAP_PHY_MIDAMBLETXRXMAXNSTS_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_MIDAMBLETXRXMAXNSTS_SET_TO_IC(ni_hecap_phyinfo, val);
#else
    val = HECAP_PHY_MIDAMBLERXMAXNSTS_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_MIDAMBLERXMAXNSTS_SET_TO_IC(ni_hecap_phyinfo, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s Midamble Rx Max NSTS Val=%x hecap->hecap_phyinfo[1]=%x"
            " hecap->hecap_phyinfo[2]=%x \n"
            ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX1],
            hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_LTFGIFORNDP_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_LTFGIFORNDP_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s NI LTF & GI NDP  Val=%x hecap->hecap_phyinfo[2]=%x \n"
         ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    /* Rx on our side and Tx on the remote side should be considered for STBC with rate control */
    val = HECAP_PHY_TXSTBC_GET_FROM_IE(&hecap_phy_ie);
    if (!(val && vap->iv_rx_stbc && (rx_streams > 1))) {
        val = 0;
    }
    HECAP_PHY_TXSTBC_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
       "%s NI TXSTBC Val=%x hecap->hecap_phyinfo[2]=%x \n"
        ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    /* Tx on our side and Rx on the remote side should be considered for STBC with rate control */
    val = HECAP_PHY_RXSTBC_GET_FROM_IE(&hecap_phy_ie);
    if (!(val && vap->iv_tx_stbc && (tx_streams > 1))) {
        val = 0;
    }
    HECAP_PHY_RXSTBC_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
       "%s NI RXSTBC Val=%x hecap->hecap_phyinfo[2]=%x \n"
        ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_TXDOPPLER_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_TXDOPPLER_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI TX Doppler Val=%x hecap->hecap_phyinfo[2]=%x \n"
         ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_RXDOPPLER_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_RXDOPPLER_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI RXDOPPLER Val=%x hecap->hecap_phyinfo[2]=%x \n"
          ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_UL_MU_MIMO_GET_FROM_IE(&hecap_phy_ie);
    if (!(val && vap->iv_he_ul_mumimo)) {
        val = 0;
    }
    HECAP_PHY_UL_MU_MIMO_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI UL MU MIMO Val=%x hecap->hecap_phyinfo[2]=%x \n"
        ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_ULOFDMA_GET_FROM_IE(&hecap_phy_ie);
    if (!(val && vap->iv_he_ul_muofdma)) {
        val = 0;
    }
    HECAP_PHY_ULOFDMA_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
         "%s UL OFDMA Val=%x  hecap->hecap_phyinfo[2]=%x \n",
         __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX2]);
    }

    val = HECAP_PHY_DCMTX_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_DCMTX_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
                "%s NI TX DCM Val=%x  hecap->hecap_phyinfo[3]=%x \n",
        __func__, val,  hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    val = HECAP_PHY_DCMRX_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_DCMRX_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI RX DCM Val=%x  hecap->hecap_phyinfo[3]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    val = HECAP_PHY_ULHEMU_PPDU_PAYLOAD_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_ULHEMU_PPDU_PAYLOAD_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
           "%s NI UL HE MU PPDU Val=%x hecap->hecap_phyinfo[3]=%x  \n"
               ,__func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    val = HECAP_PHY_SUBFMR_GET_FROM_IE(&hecap_phy_ie);
    if (!(val && vap->iv_he_su_bfee)) {
        val = 0;
    }
    HECAP_PHY_SUBFMR_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI SU BFMR Val=%x hecap->hecap_phyinfo[3]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX3]);
    }

    val = HECAP_PHY_SUBFME_GET_FROM_IE(&hecap_phy_ie);
    if (!(val && vap->iv_he_su_bfer)) {
        val = 0;
    }
    HECAP_PHY_SUBFME_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI SU BFEE Val=%x hecap->hecap_phyinfo[4]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX4]);
    }

    val = HECAP_PHY_MUBFMR_GET_FROM_IE(&hecap_phy_ie);
    if (!((vap->iv_opmode == IEEE80211_M_STA) && (val && vap->iv_he_mu_bfee))) {
        val = 0;
    }
    HECAP_PHY_MUBFMR_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI MU BFMR Val=%x hecap->hecap_phyinfo[4]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX4]);
    }

    val = HECAP_PHY_BFMENSTSLT80MHZ_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_BFMENSTSLT80MHZ_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI BFME STS LT 80 Mhz Val=%x hecap->hecap_phyinfo[4]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX4]);
    }

    val = HECAP_PHY_BFMENSTSGT80MHZ_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_BFMENSTSGT80MHZ_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI BFME STS GT 80 Mhz Val=%x hecap->hecap_phyinfo[5]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX5]);
    }

    val = HECAP_PHY_NUMSOUNDLT80MHZ_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_NUMSOUNDLT80MHZ_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI Noof Sound Dim LT 80 Mhz Val=%x hecap->hecap_phyinfo[5]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX5]);
    }

    val = HECAP_PHY_NUMSOUNDGT80MHZ_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_NUMSOUNDGT80MHZ_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI Noof Sound Dim GT 80 Mhz Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_NG16SUFEEDBACKLT80_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_NG16SUFEEDBACKLT80_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI Ng16 SU Feedback Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_NG16MUFEEDBACKGT80_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_NG16MUFEEDBACKGT80_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI Ng16 MU Feeback Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_CODBK42SU_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_CODBK42SU_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI CB SZ 4_2 SU Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_CODBK75MU_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_CODBK75MU_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI CB SZ 7_5 MU Val=%x hecap->hecap_phyinfo[6]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX6]);
    }

    val = HECAP_PHY_BFFEEDBACKTRIG_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_BFFEEDBACKTRIG_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI BF FB Trigg Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_HEERSU_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_HEERSU_SET_TO_IC(ni_hecap_phyinfo, val);
    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI HE ER SU PPDU Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_DLMUMIMOPARTIALBW_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_DLMUMIMOPARTIALBW_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI DL MUMIMO Par BW Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_PPETHRESPRESENT_GET_FROM_IE(&hecap_phy_ie);
    ppet_present = val;
    HECAP_PHY_PPETHRESPRESENT_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI PPE Thresh present Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_SRPSPRESENT_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_SRPPRESENT_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI SRPS SR Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_PWRBOOSTAR_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_PWRBOOSTAR_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI Power Boost AR Val=%x hecap->hecap_phyinfo[7]=%x \n",__func__,
         val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_4XLTFAND800NSECSGI_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_4XLTFAND800NSECSGI_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI 4X HE-LTF & 0.8 GI HE PPDU Val=%x hecap->hecap_phyinfo[7]=%x \n",
        __func__, val , hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_MAX_NC_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_MAX_NC_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s MAX Nc=%x hecap->hecap_phyinfo[7]=%x \n", __func__,
        val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_STBCTXGT80_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_STBCTXGT80_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s STBC Tx GT 80MHz=%x hecap->hecap_phyinfo[7]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_STBCRXGT80_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_STBCRXGT80_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s STBC Rx GT 80MHz=%x hecap->hecap_phyinfo[7]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX7]);
    }

    val = HECAP_PHY_ERSU_4XLTF800NSGI_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_ERSU_4XLTF800NSGI_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s ERSU 4x LTF 800 ns GI=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_HEPPDU20IN40MHZ2G_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_HEPPDU20IN40MHZ2G_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s HE PPDU 20 in 40 MHZ 2G=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_HEPPDU20IN160OR80P80MHZ_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_HEPPDU20IN160OR80P80MHZ_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s HE PPDU 20 in 160 or 80+80 MHZ=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_HEPPDU80IN160OR80P80MHZ_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_HEPPDU80IN160OR80P80MHZ_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s HE PPDU 80 in 160 or 80+80 MHZ=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_ERSU1XLTF800NSGI_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_ERSU1XLTF800NSGI_SET_TO_IC(ni_hecap_phyinfo, val);

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s ERSU 1x LTF 800 ns GI=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

#if SUPPORT_11AX_D3
    val = HECAP_PHY_MIDAMBLETXRX2XAND1XLTF_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_MIDAMBLETXRX2XAND1XLTF_SET_TO_IC(ni_hecap_phyinfo, val);
#else
    val = HECAP_PHY_MIDAMBLERX2XAND1XLTF_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_MIDAMBLERX2XAND1XLTF_SET_TO_IC(ni_hecap_phyinfo, val);
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Midamble Rx 2x and 1x LTF=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s NI hecap_macinfo = %x \n",__func__, ni_hecap_macinfo);
    }

#if SUPPORT_11AX_D3
    val = HECAP_PHY_DCMMAXBW_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_DCMMAXBW_SET_TO_IC(ni_hecap_phyinfo, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s DCM Max BW=%x hecap->hecap_phyinfo[8]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX8]);
    }

    val = HECAP_PHY_LT16HESIGBOFDMSYM_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_LT16HESIGBOFDMSYM_SET_TO_IC(ni_hecap_phyinfo, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Longer Than 16 HE SIG-B OFDM Symbols Support=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_NONTRIGCQIFDBK_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_NONTRIGCQIFDBK_SET_TO_IC(ni_hecap_phyinfo, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Non- Triggered CQI Feedback=%x hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_TX1024QAMLT242TONERU_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_TX1024QAMLT242TONERU_SET_TO_IC(ni_hecap_phyinfo, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Tx 1024- QAM < 242-tone RU Support=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_RX1024QAMLT242TONERU_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_RX1024QAMLT242TONERU_SET_TO_IC(ni_hecap_phyinfo, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Rx 1024- QAM < 242-tone RU Support=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_RXFULLBWSUHEMUPPDU_COMPSIGB_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_RXFULLBWSUHEMUPPDU_COMPSIGB_SET_TO_IC(ni_hecap_phyinfo, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Rx Full BW SU Using HE MU PPDU With Compressed SIGB=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }

    val = HECAP_PHY_RXFULLBWSUHEMUPPDU_NONCOMPSIGB_GET_FROM_IE(&hecap_phy_ie);
    HECAP_PHY_RXFULLBWSUHEMUPPDU_NONCOMPSIGB_SET_TO_IC(ni_hecap_phyinfo, val);

    if(enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s Rx Full BW SU Using HE MU PPDU With Non-Compressed SIGB=%x"
        "hecap->hecap_phyinfo[9]=%x \n",
        __func__, val, hecap->hecap_phyinfo[HECAP_PHYBYTE_IDX9]);
    }
#endif

    ieee80211_update_ht_vht_he_phymode(ic, ni);

    /* Parse NSS MCS info */
    ieee80211_hecap_parse_mcs_nss(ni, vap, hecap->hecap_txrx, enable_log,
            &mcsnssbytes);

    ni_ppet = ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0;

    if(ppet_present) {
        /* parse ie ppet and store in ni_ppet field */
        he_ppet16_ppet8_parse(ni_ppet, ((uint8_t *)hecap +
                    (HE_CAP_OFFSET_TO_PPET + mcsnssbytes)));

        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s NI HE PPET ru3_ru0[0] =%x  ru3_ru0[1]=%x _ru3_ru0[2]=%x \
             ru3_ru0[3]=%x ru3_ru0[4]=%x  ru3_ru0[5]=%x ru3_ru0[6]=%x ru3_ru0[7]=%x \n",
             __func__, ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[0],
             ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[1], ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[2],
             ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[3], ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[4],
             ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[5], ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[6],
             ni_he->hecap_ppet.ppet16_ppet8_ru3_ru0[7]);
        }
    }

}

static void
ieee80211_add_he_vhtop(u_int8_t *frm, struct ieee80211_node *ni,
                    struct ieee80211com *ic,  u_int8_t subtype,
                    struct ieee80211_framing_extractx *extractx)
{
    struct ieee80211vap *vap = ni->ni_vap;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    u_int8_t chwidth = 0, negotiate_bw = 0;
    u_int8_t *frm_chwidth = frm, *frm_cfreq_seg1 = frm+1, *frm_cfreq_seg2 = frm+2;


    if((ni->ni_160bw_requested == 1) &&
       (IEEE80211_IS_CHAN_11AC_VHT160(ic->ic_curchan) ||
        IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan)) &&
        (ni->ni_chwidth == IEEE80211_VHTOP_CHWIDTH_80) &&
        (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) {
        /* Set negotiated BW */
        chwidth = ni->ni_chwidth;
        negotiate_bw = 1;
    } else {
       if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
           chwidth = vap->iv_chwidth;
       } else {
           chwidth = ic_cw_width;
       }
    }

    /* Fill in the VHT Operation info */
    if (chwidth == IEEE80211_CWM_WIDTH160) {
        if (vap->iv_rev_sig_160w) {
            if(IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan))
                *frm_chwidth = IEEE80211_VHTOP_CHWIDTH_REVSIG_80_80;
            else
               *frm_chwidth = IEEE80211_VHTOP_CHWIDTH_REVSIG_160;
        } else {
            if(IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan))
                *frm_chwidth = IEEE80211_VHTOP_CHWIDTH_80_80;
            else
                *frm_chwidth = IEEE80211_VHTOP_CHWIDTH_160;
        }
    }
    else if (chwidth == IEEE80211_CWM_WIDTH80)
        *frm_chwidth = IEEE80211_VHTOP_CHWIDTH_80;
    else
        *frm_chwidth = IEEE80211_VHTOP_CHWIDTH_2040;

    if (negotiate_bw == 1) {

            *frm_cfreq_seg1 = vap->iv_bsschan->ic_vhtop_ch_freq_seg1;
            /* Note: This is applicable only for 80+80Mhz mode */
            *frm_cfreq_seg2 = 0;
    }
    else {
        if (chwidth == IEEE80211_CWM_WIDTH160) {

            if (vap->iv_rev_sig_160w) {
                /* Our internal channel structure is in sync with
                 * revised 160 MHz signalling. So use seg1 and
                 * seg2 directly for 80_80 and 160.
                 */
                 *frm_cfreq_seg1=
                    vap->iv_bsschan->ic_vhtop_ch_freq_seg1;

                *frm_cfreq_seg2 =
                    vap->iv_bsschan->ic_vhtop_ch_freq_seg2;
            } else {
                /* Use legacy 160 MHz signaling */
                if(IEEE80211_IS_CHAN_11AC_VHT160(ic->ic_curchan)) {
                    /* ic->ic_curchan->ic_vhtop_ch_freq_seg2 is centre
                     * frequency for whole 160 MHz.
                     */
                    *frm_cfreq_seg1 =
                        vap->iv_bsschan->ic_vhtop_ch_freq_seg2;
                    *frm_cfreq_seg2 = 0;
                } else {
                    /* 80 + 80 MHz */
                    *frm_cfreq_seg1 =
                        vap->iv_bsschan->ic_vhtop_ch_freq_seg1;

                    *frm_cfreq_seg2 =
                        vap->iv_bsschan->ic_vhtop_ch_freq_seg2;
                }
            }
       } else { /* 80MHZ or less */
            *frm_cfreq_seg1 = vap->iv_bsschan->ic_vhtop_ch_freq_seg1;
            *frm_cfreq_seg2 = 0;
        }
    }


    /* We currently honor a 160 MHz association WAR request from callers only for
     * IEEE80211_FC0_SUBTYPE_PROBE_RESP and IEEE80211_FC0_SUBTYPE_ASSOC_RESP, and
     * check if our current channel is for 160/80+80 MHz.
     */
    if ((!vap->iv_rev_sig_160w) && (extractx != NULL) &&
        (extractx->fectx_assocwar160_reqd == true) &&
        ((subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) ||
            (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP)) &&
        (IEEE80211_IS_CHAN_11AC_VHT160(ic->ic_curchan) ||
            IEEE80211_IS_CHAN_11AC_VHT80_80(ic->ic_curchan))) {
       /* Remove all indications of 160 MHz capability, to enable the STA to
        * associate.
        *
        * Besides, we set vht_op_chwidth to IEEE80211_VHTOP_CHWIDTH_80 without
        * checking if preceeding code had set it to lower value negotiated with
        * STA. This is for logical conformance with older VHT AP behaviour
        * wherein width advertised would remain constant across probe response
        * and assocation response.
        */

        /* Downgrade to 80 MHz */
        *frm_chwidth = IEEE80211_VHTOP_CHWIDTH_80;

        *frm_cfreq_seg1 = vap->iv_bsschan->ic_vhtop_ch_freq_seg1;
        *frm_cfreq_seg2 = 0;
    }

}

/**
* @brief    Sets the HE-Op Basic HE-MCS and NSS-Set.
*           This function sets the HE-MCS 'mcs' for the
*           requested number of spatial streams 'nss' in the
*           Basic HE-MCS and NSS Set in the HE-Op element.
*
* @param vap: handle to vap
* @param nss: number of ss for which HE-MCS 0-11 needs to be set
*
* @return 32 bit value containing the desired bit-pattern
*/
static uint16_t
ieee80211_set_heop_basic_mcs_nss_set(uint8_t mcs, uint8_t nss)
{
    u_int16_t heop_mcs_nss = 0x0;
    u_int8_t  i;

    if ((mcs < HE_MCS_VALUE_INVALID) && (nss < MAX_HE_NSS)) {
        for (i = 0; i < nss; i++) {
            /* Set the desired 2-bit value and left
             * shift by 2-bits if we are left with
             * another ss to set this value
             */
            heop_mcs_nss |= (mcs << (i*HE_NBITS_PER_SS_IN_HE_MCS_NSS_SET));
        }

        /* Clear nss*2 bits in the bit sequence
         * RESRVD_BITS_FOR_ALL_SS_IN_HE_MCS_NSS_SET
         * so that we can or resultant bit sequence
         * with heop_mcs_nss (set above)
         */
        heop_mcs_nss |= (HE_RESRVD_BITS_FOR_ALL_SS_IN_HE_MCS_NSS_SET &
                           (~((1 << i*HE_NBITS_PER_SS_IN_HE_MCS_NSS_SET) - 1)));
    } else {
        qdf_err("%s WARNING!!! SS more than %d not supported", __func__, MAX_HE_NSS);
    }

    return heop_mcs_nss;
}

/**
* @brief    Get the default-max PE duration.
*           This function gets the default max PPE duration from
*           target sent hecap_ppet values
*
* @param tot_nss            total number of SS
* @param ru_mask            ru mask as sent by the target
* @param ppet16_ppet8       pointer to target sent ppet values
*
* @return 8 bit value containing the default-max PPE duration
*/
static u_int8_t
he_get_default_max_pe_duration(u_int8_t tot_nss,
                         u_int32_t ru_mask, u_int32_t *ppet16_ppet8) {

    u_int8_t ppet8_val, ppet16_val;
    u_int8_t tot_ru                  = HE_PPET_TOT_RU_BITS;
    u_int8_t default_max_pe_duration = IEEE80211_HE_PE_DURATION_NONE;
    u_int8_t nss, ru, ci;
    u_int32_t temp_ru_mask;

    /* Need to break out of both the loops as soon as the max
     * value is hit. Therefore, both the loop carries the check
     * against the max values
     */
    for(nss=0; nss < tot_nss &&
            default_max_pe_duration < IEEE80211_HE_PE_DURATION_MAX;
                nss++) {    /* loop NSS */
        temp_ru_mask = ru_mask;

        /* Break out of the loop as soon as max values is hit */
        for(ru=1; ru <= tot_ru &&
            default_max_pe_duration < IEEE80211_HE_PE_DURATION_MAX;
                ru++) {   /* loop RU */

            if(temp_ru_mask & 0x1) {

                /* extract ppet16 & ppet8 from IC he ppet handle */
                ppet16_val = HE_GET_PPET16(ppet16_ppet8, ru, nss);
                ppet8_val  = HE_GET_PPET8(ppet16_ppet8, ru, nss);

                /* Break out of the loop as soon as max values is hit */
                for (ci = 0; ci < HE_PPET_MAX_CI &&
                    default_max_pe_duration < IEEE80211_HE_PE_DURATION_MAX; ci++) {

                   /* Refer to Table 27-8: PPE thresholds per PPET8 and PPET16
                    * for the pe-duration derviation scheme used below
                    */
                    if ((ci >= ppet8_val) &&
                            ((ci < ppet16_val) || (ppet16_val == HE_PPET_NONE))) {
                        default_max_pe_duration = IEEE80211_HE_PE_DURATION_8US;
                    }

                    if (((ci > ppet8_val) ||
                        (ppet8_val == HE_PPET_NONE)) && (ci >= ppet16_val)) {
                        default_max_pe_duration = IEEE80211_HE_PE_DURATION_MAX;
                    }
                }

            }
            temp_ru_mask = temp_ru_mask >> 1;
        }
    }

    return default_max_pe_duration;
}

#if SUPPORT_11AX_D3
struct he_op_bsscolor_info
ieee80211_get_he_bsscolor_info(struct ieee80211vap *vap) {
    struct he_op_bsscolor_info hebsscolor_info;
    struct ieee80211com *ic = vap->iv_ic;

    hebsscolor_info.bss_color       = (ic->ic_he.heop_bsscolor_info &
                                      IEEE80211_HEOP_BSS_COLOR_MASK) >>
                                      IEEE80211_HEOP_BSS_COLOR_S;
    hebsscolor_info.part_bss_color  = (ic->ic_he.heop_bsscolor_info &
                                      IEEE80211_HEOP_PARTIAL_BSS_COLOR_MASK) >>
                                      IEEE80211_HEOP_PARTIAL_BSS_COLOR_S;
    hebsscolor_info.bss_color_dis   = (ic->ic_he.heop_bsscolor_info &
                                      IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK) >>
                                      IEEE80211_HEOP_BSS_COLOR_DISABLD_S;

    /* Fill the bss color value from the IC as
     * it is the generic bss color for all VAPs -
     * ieee80211AX: Section 27.11.4 - "All APs
     * that are members of a multiple
     * BSSID set shall use the same BSS color".
     * The bss color set from the userspace
     * in the vap structure is taken care while
     * setting up the bss color in IC through
     * ieee80211_setup_bsscolor().
     */
    if (!vap->iv_he_bsscolor_change_ongoing) {
        hebsscolor_info.bss_color = ic->ic_bsscolor_hdl.selected_bsscolor;

        if (ieee80211_is_bcca_ongoing_for_any_vap(ic)) {
            hebsscolor_info.bss_color_dis = IEEE80211_HE_BSS_COLOR_ENABLE;
        } else {
           /* The disabled bss color bit in the ic_heop_param is set and
            * assigned the value '1' when the user disables BSS color. This
            * assignment happens in the set bss color handler and the assigned
            * value is populated in the heop param here. */
            hebsscolor_info.bss_color_dis = (ic->ic_he.heop_bsscolor_info &
                                    IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK) >>
                                    IEEE80211_HEOP_BSS_COLOR_DISABLD_S;
        }
    } else {
        /* Keep advertising disabled bss color till bsscolor change
         * switch count is 0
         */
        hebsscolor_info.bss_color = ic->ic_bsscolor_hdl.prev_bsscolor;
        hebsscolor_info.bss_color_dis = !IEEE80211_HE_BSS_COLOR_ENABLE;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE,
    "%s he_bsscolor_info:> bss_color=%x, part_bss_color=%x, bss_color_dis=%x",
    __func__, hebsscolor_info.bss_color, hebsscolor_info.part_bss_color,
    hebsscolor_info.bss_color_dis);

    return hebsscolor_info;
}
EXPORT_SYMBOL(ieee80211_get_he_bsscolor_info);
#endif

struct he_op_param
ieee80211_get_heop_param(struct ieee80211vap *vap) {
    struct he_op_param heop_param;
    struct ieee80211com *ic = vap->iv_ic;
    uint8_t rx_streams      = ieee80211_get_rxstreams(ic, vap);
    uint8_t tx_streams      = ieee80211_get_txstreams(ic, vap);
    uint8_t nss             = MIN(rx_streams, tx_streams);

   /* Fill in the default HE OP info from IC */
#if !SUPPORT_11AX_D3
    heop_param.bss_color           = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_BSS_COLOR_MASK) >>
                                        IEEE80211_HEOP_BSS_COLOR_S;
#endif
    heop_param.def_pe_dur          = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_DEFAULT_PE_DUR_MASK) >>
                                        IEEE80211_HEOP_DEFAULT_PE_DUR_S;
    heop_param.twt_required        = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_TWT_REQUIRED_MASK) >>
                                        IEEE80211_HEOP_TWT_REQUIRED_S;
    heop_param.rts_threshold       = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_RTS_THRESHOLD_MASK) >>
                                        IEEE80211_HEOP_RTS_THRESHOLD_S;
#if !SUPPORT_11AX_D3
    heop_param.part_bss_color      = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_PARTIAL_BSS_COLOR_MASK) >>
                                        IEEE80211_HEOP_PARTIAL_BSS_COLOR_S;
#endif
    heop_param.vht_op_info_present = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_VHTOP_PRESENT_MASK) >>
                                        IEEE80211_HEOP_VHTOP_PRESENT_S;
#if SUPPORT_11AX_D3
    heop_param.co_located_bss      = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_CO_LOCATED_BSS_MASK) >>
                                        IEEE80211_HEOP_CO_LOCATED_BSS_S;
    heop_param.er_su_disable       = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_ER_SU_DISABLE_MASK) >>
                                        IEEE80211_HEOP_ER_SU_DISABLE_S;
    heop_param.reserved            = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_RESERVED_MASK) >>
                                        IEEE80211_HEOP_RESERVED_S;
#else
    heop_param.reserved            = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_RESERVED_MASK) >>
                                        IEEE80211_HEOP_RESERVED_S;
    heop_param.multiple_bssid_ap   = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_MULT_BSSID_AP_MASK) >>
                                        IEEE80211_HEOP_MULT_BSSID_AP_S;
    heop_param.tx_mbssid           = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_TX_MBSSID_MASK) >>
                                        IEEE80211_HEOP_TX_MBSSID_S;
    heop_param.reserved_1          = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_RESERVED1_MASK) >>
                                        IEEE80211_HEOP_RESERVED1_S;
    heop_param.bss_color_dis       = (ic->ic_he.heop_param &
                                     IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK) >>
                                        IEEE80211_HEOP_BSS_COLOR_DISABLD_S;

   /* Fill the bss color value from the IC as
    * it is the generic bss color for all VAPs -
    * ieee80211AX: Section 27.11.4 - "All APs
    * that are members of a multiple
    * BSSID set shall use the same BSS color".
    * The bss color set from the userspace
    * in the vap structure is taken care while
    * setting up the bss color in IC through
    * ieee80211_setup_bsscolor().
    */
    if (!vap->iv_he_bsscolor_change_ongoing) {
        heop_param.bss_color = ic->ic_bsscolor_hdl.selected_bsscolor;

        if (ieee80211_is_bcca_ongoing_for_any_vap(ic)) {
            heop_param.bss_color_dis = IEEE80211_HE_BSS_COLOR_ENABLE;
        } else {
            /* The disabled bss color bit in the ic_heop_param is set and assigned
            * the value '1' when the user disables BSS color. This assignment happens
            * in the set bss color handler and the assigned value is populated in the
            * heop param here. */
            heop_param.bss_color_dis = (ic->ic_he.heop_param &
                                    IEEE80211_HEOP_BSS_COLOR_DISABLD_MASK) >>
                                    IEEE80211_HEOP_BSS_COLOR_DISABLD_S;
        }
    } else {
        /* Keep advertising disabled bss color till bsscolor change
         * switch count is 0
         */
        heop_param.bss_color = ic->ic_bsscolor_hdl.prev_bsscolor;
        heop_param.bss_color_dis = !IEEE80211_HE_BSS_COLOR_ENABLE;
    }

#endif
   /* Find the default max PPE duration from
    * target sent hecap_ppet values and fill
    * in as the default PE duration
    */
    heop_param.def_pe_dur = he_get_default_max_pe_duration(nss,
                                ic->ic_he.hecap_ppet.ru_mask,
                                    ic->ic_he.hecap_ppet.ppet16_ppet8_ru3_ru0);

    heop_param.rts_threshold = vap->iv_he_rts_threshold;

#if !SUPPORT_11AX_D3
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE,
    "%s heop_params:> bss_color=%x, default_pe_durn=%x, "
    "twt_required=%x, rts_thrshld=%x, part_bss_color=%x, "
    "vht_op_info_present=%x, reserved=%x, multiple_bssid_ap=%x, "
    "tx_mbssid=%x, bss_color_dis=%x, reserved_1=%x\n"
    , __func__, heop_param.bss_color, heop_param.def_pe_dur,
    heop_param.twt_required, heop_param.rts_threshold,
    heop_param.part_bss_color, heop_param.vht_op_info_present,
    heop_param.reserved, heop_param.multiple_bssid_ap,
    heop_param.tx_mbssid, heop_param.bss_color_dis,
    heop_param.reserved_1);
#else
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE,
    "%s heop_params:> default_pe_durn=%x, "
    "twt_required=%x, rts_thrshld=%x, "
    "vht_op_info_present=%x, er_su_disable=%x, co_located_bss=%x, "
    "reserved=%x\n"
    , __func__, heop_param.def_pe_dur,
    heop_param.twt_required, heop_param.rts_threshold,
    heop_param.vht_op_info_present,
    heop_param.er_su_disable, heop_param.co_located_bss,
    heop_param.reserved);
#endif

    return heop_param;
}
EXPORT_SYMBOL(ieee80211_get_heop_param);

uint8_t *
ieee80211_add_heop(u_int8_t *frm, struct ieee80211_node *ni,
                  struct ieee80211com *ic, u_int8_t subtype,
                  struct ieee80211_framing_extractx *extractx)
{
    struct ieee80211_ie_heop *heop = (struct ieee80211_ie_heop *)frm;
    struct ieee80211vap      *vap  = ni->ni_vap;
    bool enable_log      = false;
    int heoplen          = sizeof(struct ieee80211_ie_heop);
#if SUPPORT_11AX_D3
    struct he_op_bsscolor_info heop_bsscolor_info;
#endif
    struct he_op_param heop_param;
    u_int16_t heop_mcs_nss;

    if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
         subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) {

         enable_log = true;
    }

    heop->elem_id     = IEEE80211_ELEMID_EXTN;
    /* elem id + len = 2 bytes, readjust based on ppet */
    heop->elem_len    = sizeof(struct ieee80211_ie_heop) -
                                                IEEE80211_IE_HDR_LEN;
    heop->elem_id_ext = IEEE80211_ELEMID_EXT_HEOP;

    heop_param = ieee80211_get_heop_param(vap);
    qdf_mem_copy(heop->heop_param, &heop_param, sizeof(heop->heop_param));

#if SUPPORT_11AX_D3
    heop_bsscolor_info = ieee80211_get_he_bsscolor_info(vap);
    qdf_mem_copy(&heop->heop_bsscolor_info, &heop_bsscolor_info,
            sizeof(heop->heop_bsscolor_info));

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s IC HE OP=%x Frame HE OP Params =%x%x%x%x\n"
        , __func__, ic->ic_he.heop_param, heop->heop_param[0],
        heop->heop_param[1], heop->heop_param[2], heop->heop_bsscolor_info);
    }
#else
    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s IC HE OP=%x Frame HE OP Params =%x%x%x%x\n"
        , __func__, ic->ic_he.heop_param, heop->heop_param[0],
        heop->heop_param[1], heop->heop_param[2], heop->heop_param[3]);
    }
#endif

    heop_mcs_nss = ieee80211_set_heop_basic_mcs_nss_set
                    (HE_MCS_VALUE_FOR_MCS_0_7, HE_DEFAULT_SS_IN_MCS_NSS_SET);

    qdf_mem_copy(heop->heop_mcs_nss, &heop_mcs_nss,
                                 sizeof(heop->heop_mcs_nss));

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
        "%s IC HE MCS NSS =%x HE OP MCS NSS =%x Frame HEOP= %x%x\n"
        , __func__, ic->ic_he.hecap_rxmcsnssmap, heop_mcs_nss,
                heop->heop_mcs_nss[0],heop->heop_mcs_nss[1]);
    }

    if(heop_param.vht_op_info_present) {
        /* Fill in VHT Operation - Width, Cfreq1, Cfreq2 */
        ieee80211_add_he_vhtop(heop->heop_vht_opinfo,
                                ni, ic, subtype, extractx);
        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s Frame HE OP VHT Info Width =%x Cfre1=%x Cfreq2=%x\n"
            , __func__, heop->heop_vht_opinfo[0],
            heop->heop_vht_opinfo[1], heop->heop_vht_opinfo[2]);
        }
    } else {
        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s HE OP VHT Info not present\n", __func__);
        }
        heoplen        -= HEOP_VHT_OPINFO;
        heop->elem_len -= HEOP_VHT_OPINFO;
    }
#if SUPPORT_11AX_D3
    if(!heop_param.co_located_bss) {
#else
    if(!heop_param.multiple_bssid_ap) {
#endif
        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
            "%s HE OP Max BSSID indicator not present\n", __func__);
        }
        heoplen        -= HEOP_MAX_BSSID_INDICATOR;
        heop->elem_len -= HEOP_MAX_BSSID_INDICATOR;
    }

    return frm + heoplen;
}

static void
ieee80211_parse_he_vhtop(struct ieee80211_node *ni, u_int8_t *ie)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int ch_width;
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    u_int8_t *vhtop_chwidth = ie, *vhtop_cfreq_seg1 = ie+1,  *vhtop_cfreq_seg2 = ie+2;

    switch (*vhtop_chwidth) {
       case IEEE80211_VHTOP_CHWIDTH_2040:
           /* Exact channel width is already taken care of by the HT parse */
       break;
       case IEEE80211_VHTOP_CHWIDTH_80:
       if(((*vhtop_chwidth == IEEE80211_VHTOP_CHWIDTH_REVSIG_160) &&
          (*vhtop_cfreq_seg1 != 0) && (abs(*vhtop_cfreq_seg2 -
           *vhtop_cfreq_seg1) == 8)) ||
           ((*vhtop_chwidth == IEEE80211_VHTOP_CHWIDTH_REVSIG_80_80) &&
           (*vhtop_cfreq_seg1 != 0) && (abs(*vhtop_cfreq_seg2 -
           *vhtop_cfreq_seg1) > 8))) {

           ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
       }
       else {
           ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
       }
       break;
       case IEEE80211_VHTOP_CHWIDTH_160:
       case IEEE80211_VHTOP_CHWIDTH_80_80:
           ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
       break;
       default:
           IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_DEBUG,
                            "%s: Unsupported Channel Width\n", __func__);
       break;
    }

    if (vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID) {
        ch_width = vap->iv_chwidth;
    } else {
        ch_width = ic_cw_width;
    }
    /* Update ch_width only if it is within the user configured width*/
    if(ch_width < ni->ni_chwidth) {
        ni->ni_chwidth = ch_width;
    }
}

void
ieee80211_parse_heop(struct ieee80211_node *ni, u_int8_t *ie, u_int8_t subtype)
{
    struct ieee80211_ie_heop *heop    = (struct ieee80211_ie_heop *)ie;
    struct ieee80211_he_handle *ni_he = &ni->ni_he;
    struct ieee80211vap *vap         = ni->ni_vap;
    uint16_t basic_mcsnssmap;
    bool enable_log = false;

    if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
         subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) {
         enable_log = true;
    }

    /* Negotiated HE OP Params  */
    qdf_mem_copy(&ni_he->heop_param, heop->heop_param,
                                    sizeof(heop->heop_param));

#if SUPPORT_11AX_D3
    qdf_mem_copy(&ni_he->heop_bsscolor_info, &heop->heop_bsscolor_info,
                                    sizeof(heop->heop_bsscolor_info));
#endif

    if (enable_log) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
       "%s NI OP Params =%x \n", __func__, ni_he->heop_param);
    }

    if (vap->iv_opmode != IEEE80211_M_HOSTAP) {
        (void ) qdf_get_u16(heop->heop_mcs_nss, &basic_mcsnssmap);
        /* Derive basic mcs and nss requirement for the bss */
        HE_DERIVE_PEER_NSS_FROM_MCSMAP(basic_mcsnssmap,
                                    vap->iv_he_basic_nss_for_bss);
        /* Derive basic mcs requirement from first stream only.
         * Limiting it to first stream as of now as most AP vendors
         * advertises one stream mcsmap as the basic requirement.
         * In future, we may have to extend this if we encounter
         * HEOP with more than 1ss basic requirement.
         */
        vap->iv_he_basic_mcs_for_bss = basic_mcsnssmap & 0x03;
    }

    if (ni_he->heop_param & IEEE80211_HEOP_VHTOP_PRESENT_MASK) {
        /* Negotiated HE VHT OP */
        ieee80211_parse_he_vhtop(ni, heop->heop_vht_opinfo);
        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
           "%s NI OP Params =%x %x %x\n", __func__, heop->heop_vht_opinfo[0],
            heop->heop_vht_opinfo[1], heop->heop_vht_opinfo[2]);
        }
    } else {
        if (enable_log) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_HE,
           "%s HEOP VHT Info not present in IE\n", __func__);
        }
    }
}

/* extnss_160_validate_and_seg2_indicate() - Validate vhtcap if EXT NSS supported
 * @arg2 - vhtcap
 * @arg3 - vhtop
 * @arg4 - htinfo
 *
 * Function to validate vht capability combination of "supported chwidth" and "ext nss support"
 * along with indicating appropriate location to retrieve seg2 from(either htinfo or vhtop).
 * This is a helper function. It assumed that non-NULL pointers are passed.
 *
 * Return - 1 : If seg2 is to be extracted from vhtop
 *          2 : If seg2 is to be extracted from htinfo
 *          0 : Failure
 */
u_int8_t extnss_160_validate_and_seg2_indicate(u_int32_t *vhtcap, struct ieee80211_ie_vhtop *vhtop, struct ieee80211_ie_htinfo_cmn *htinfo)
{

    if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_00_80F1_160NONE_80P80NONE)) {
        return 0;
    }

    if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_13_80F2_160F2_80P80F1) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_10_80F1_160F1_80P80NONE) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_11_80F1_160F1_80P80FDOT5) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_12_80F1_160F1_80P80FDOT75) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_23_80F2_160F1_80P80F1)) {
        if ((vhtop->vht_op_chwidth == IEEE80211_VHTOP_CHWIDTH_REVSIG_160) &&
        (vhtop->vht_op_ch_freq_seg2 != 0) &&
        (abs(vhtop->vht_op_ch_freq_seg2 - vhtop->vht_op_ch_freq_seg1) == 8)) {
            return 1;
        }
    } else if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_01_80F1_160FDOT5_80P80NONE) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75)) {
       if ((vhtop->vht_op_chwidth == IEEE80211_VHTOP_CHWIDTH_REVSIG_160) &&
        (HTINFO_CCFS2_GET(htinfo) != 0) &&
        (abs(HTINFO_CCFS2_GET(htinfo) - vhtop->vht_op_ch_freq_seg1) == 8)) {
           return 2;
        }
    } else {
        return 0;
    }
      return 0;
}

/*  retrieve_seg2_for_extnss_160() - Retrieve seg2 based on vhtcap
 * @arg2 - vhtcap
 * @arg3 - vhtop
 * @arg4 - htinfo
 *
 * Function to retrieve seg2 from either vhtop or htinfo based on the
 * vhtcap advertised by the AP.
 * This is a helper function. It assumed that non-NULL pointers are passed.
 *
 * Return - vht_op_ch_freq_seg2
 */
u_int32_t  retrieve_seg2_for_extnss_160(u_int32_t *vhtcap, struct ieee80211_ie_vhtop *vhtop, struct ieee80211_ie_htinfo_cmn *htinfo)
{
    u_int8_t val;

    val = extnss_160_validate_and_seg2_indicate(vhtcap, vhtop, htinfo);
    if (val == 1) {
       return vhtop->vht_op_ch_freq_seg2;
    } else if (val == 2) {
       return  HTINFO_CCFS2_GET(htinfo);
    } else {
       return 0;
    }
}

/* extnss_80p80_validate_and_seg2_indicate() - Validate vhtcap if EXT NSS supported
 * @arg2 - vhtcap
 * @arg3 - vhtop
 * @arg4 - htinfo
 *
 * Function to validate vht capability combination of "supported chwidth" and "ext nss support"
 * along with along with indicating appropriate location to retrieve seg2 from(either htinfo or vhtop)
 * This is a helper function. It assumed that non-NULL pointers are passed.
 *
 * Return - 1 : If seg2 is to be extracted from vhtop
 *          2 : If seg2 is to be extracted from htinfo
 *          0 : Failure
 */
u_int8_t extnss_80p80_validate_and_seg2_indicate(u_int32_t *vhtcap, struct ieee80211_ie_vhtop *vhtop, struct ieee80211_ie_htinfo_cmn *htinfo)
{

    if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_13_80F2_160F2_80P80F1) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_23_80F2_160F1_80P80F1)) {
        if ((vhtop->vht_op_chwidth == IEEE80211_VHTOP_CHWIDTH_REVSIG_80_80) &&
        (vhtop->vht_op_ch_freq_seg2 != 0) &&
        (abs(vhtop->vht_op_ch_freq_seg2 - vhtop->vht_op_ch_freq_seg1) > 16)) {
            return 1;
        }
    } else if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_11_80F1_160F1_80P80FDOT5) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_12_80F1_160F1_80P80FDOT75) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75)) {
        if ((vhtop->vht_op_chwidth == IEEE80211_VHTOP_CHWIDTH_REVSIG_80_80) &&
        (HTINFO_CCFS2_GET(htinfo) != 0) &&
        (abs(HTINFO_CCFS2_GET(htinfo) - vhtop->vht_op_ch_freq_seg1) > 16)) {
            return 2;
        }
    } else {
        return 0;
    }
    return 0;
}

/*  retrieve_seg2_for_extnss_80p80() - Retrieve seg2 based on vhtcap
 * @arg1 - struct ieee80211vap
 * @arg2 - vhtcap
 * @arg3 - vhtop
 * @arg4 - htinfo
 *
 * Function to retrieve seg2 from either vhtop or htinfo based on the
 * vhtcap advertised by the AP.
 * This is a helper function. It assumed that non-NULL pointers are passed.
 *
 * Return - seg2 if present in vhtop or htinfo.
 */
u_int8_t  retrieve_seg2_for_extnss_80p80(u_int32_t *vhtcap, struct ieee80211_ie_vhtop *vhtop, struct ieee80211_ie_htinfo_cmn *htinfo)
{
    u_int8_t val;

    val = extnss_80p80_validate_and_seg2_indicate(vhtcap, vhtop, htinfo);
    if (val == 1) {
       return vhtop->vht_op_ch_freq_seg2;
    } else if (val == 2) {
       return  HTINFO_CCFS2_GET(htinfo);
    } else {
       return 0;
    }
}

/* validate_extnss_vhtcap() - Validate for valid combinations in vhtcap
 * @arg2 - vhtcap
 *
 * Function to validate vht capability combination of "supported chwidth"
 * and "ext nss support" advertised by STA.
 * This is a helper function. It assumed that non-NULL pointers are passed.
 *
 * Return - true : BW 160 supported
 *          false : Failure
 */
bool validate_extnss_vhtcap(u_int32_t *vhtcap)
{
        if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_00_80F1_160NONE_80P80NONE) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_01_80F1_160FDOT5_80P80NONE) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) ==  IEEE80211_EXTNSS_MAP_10_80F1_160F1_80P80NONE ) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_11_80F1_160F1_80P80FDOT5) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_12_80F1_160F1_80P80FDOT75) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_13_80F2_160F2_80P80F1) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_23_80F2_160F1_80P80F1)) {
                   return true;
        }
    return false;
}
/* ext_nss_160_supported() - Validate 160MHz support for EXT NSS supported STA
 * @arg2 - vhtcap
 *
 * Function to validate vht capability combination of "supported chwidth"
 * and "ext nss support" advertised by STA for 160MHz.
 * This is a helper function. It assumed that non-NULL pointers are passed.
 *
 * Return - true : BW 160 supported
 *          false : Failure
 */
bool ext_nss_160_supported(u_int32_t *vhtcap)
{
        if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_01_80F1_160FDOT5_80P80NONE) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) ==  IEEE80211_EXTNSS_MAP_10_80F1_160F1_80P80NONE ) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_11_80F1_160F1_80P80FDOT5) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_12_80F1_160F1_80P80FDOT75) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_13_80F2_160F2_80P80F1) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_23_80F2_160F1_80P80F1)) {
                   return true;
        }
    return false;
}

/* ext_nss_80p80_supported() - Validate 160MHz support for EXT NSS supported STA
 * @arg2 - vhtcap
 *
 * Function to validate vht capability combination of "supported chwidth"
 * and "ext nss support" advertised by STA fo 80+80 MHz.
 * This is a helper function. It assumed that non-NULL pointers are passed.
 *
 * Return - true : BW 80+80 supported
 *          false : Failure
 */
bool ext_nss_80p80_supported(u_int32_t *vhtcap)
{
        if (((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_02_80F1_160FDOT5_80P80FDOT5 ) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_03_80F1_160FDOT75_80P80FDOT75) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_11_80F1_160F1_80P80FDOT5) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_12_80F1_160F1_80P80FDOT75) ||
                ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_13_80F2_160F2_80P80F1) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_20_80F1_160F1_80P80F1) ||
               ((*vhtcap & IEEE80211_VHTCAP_EXT_NSS_MASK) == IEEE80211_EXTNSS_MAP_23_80F2_160F1_80P80F1)) {
                   return true;
        }
    return false;
}

/* peer_ext_nss_capable() - Validate peer EXT NSS capability
 * @arg1 - vhtcap
 *
 * Function to validate if peer is capable of EXT NSS Signaling
 *
 * Return - true : Peer is capable of EXT NSS
 *        - false : Peer not capable of EXT NSS
 */
bool peer_ext_nss_capable(struct ieee80211_ie_vhtcap * vhtcap)
{
    struct supp_tx_mcs_extnss tx_mcs_extnss_cap;
    OS_MEMCPY(&tx_mcs_extnss_cap, &vhtcap->tx_mcs_extnss_cap, sizeof(u_int16_t));
    *(u_int16_t *)&tx_mcs_extnss_cap = le16toh(*(u_int16_t*)&tx_mcs_extnss_cap);
    if (tx_mcs_extnss_cap.ext_nss_capable) {
        return true;
    }
    return false;
}


#if DBDC_REPEATER_SUPPORT

#define IE_CONTENT_SIZE 1

/*
 * Add Extender information element to a frame
 */
u_int8_t *
ieee80211_add_extender_ie(struct ieee80211vap *vap, ieee80211_frame_type ftype, u_int8_t *frm)
{
    u_int8_t *ie_len;
    u_int8_t i;
    struct ieee80211com *ic = vap->iv_ic;
    struct global_ic_list *ic_list = ic->ic_global_list;
    struct ieee80211com *tmp_ic = NULL;
    struct ieee80211vap *tmpvap = NULL;
    u_int8_t extender_info = ic_list->extender_info;
    u_int8_t apvaps_cnt = 0, stavaps_cnt = 0;
    u_int8_t *pos1, *pos2;
    static const u_int8_t oui[4] = {
        (QCA_OUI & 0xff), ((QCA_OUI >> 8) & 0xff), ((QCA_OUI >> 16) & 0xff),
        QCA_OUI_EXTENDER_TYPE
    };

    *frm++ = IEEE80211_ELEMID_VENDOR;
    ie_len = frm;
    *frm++ = sizeof(oui) + IE_CONTENT_SIZE;
    OS_MEMCPY(frm, oui, sizeof(oui));
    frm += sizeof(oui);
    *frm++ = extender_info;

    if (ftype == IEEE80211_FRAME_TYPE_ASSOCRESP) {
        pos1 = frm++;
        pos2 = frm++;
        *ie_len += 2;
        for (i = 0; i < MAX_RADIO_CNT; i++) {
            GLOBAL_IC_LOCK_BH(ic_list);
            tmp_ic = ic_list->global_ic[i];
            GLOBAL_IC_UNLOCK_BH(ic_list);
            if(tmp_ic) {
                TAILQ_FOREACH(tmpvap, &tmp_ic->ic_vaps, iv_next) {
                    if (tmpvap->iv_opmode == IEEE80211_M_HOSTAP) {
                        OS_MEMCPY(frm, tmpvap->iv_myaddr, IEEE80211_ADDR_LEN);
                        frm += IEEE80211_ADDR_LEN;
                        apvaps_cnt++;
                    }
                }
            }
        }
        for (i = 0; i < MAX_RADIO_CNT; i++) {
            GLOBAL_IC_LOCK_BH(ic_list);
            tmp_ic = ic_list->global_ic[i];
            GLOBAL_IC_UNLOCK_BH(ic_list);
            if(tmp_ic) {
                if (tmp_ic->ic_sta_vap) {
                    /*Copy only MPSTA mac address, not PSTA mac address*/
                    tmpvap = tmp_ic->ic_sta_vap;
                    OS_MEMCPY(frm, tmpvap->iv_myaddr, IEEE80211_ADDR_LEN);
                    frm += IEEE80211_ADDR_LEN;
                    stavaps_cnt++;
                }
            }
        }
        *pos1 = apvaps_cnt;
        *pos2 = stavaps_cnt;
        *ie_len += ((apvaps_cnt + stavaps_cnt) * IEEE80211_ADDR_LEN);
    }

    return frm;
}

void
ieee80211_process_extender_ie(struct ieee80211_node *ni, const u_int8_t *ie, ieee80211_frame_type ftype)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    struct global_ic_list *ic_list = ic->ic_global_list;
    u_int8_t ie_len, i;
    u_int8_t *mac_list;
    u_int8_t extender_ie_content, extender_ie_type;
    u_int8_t apvaps_cnt = 0,stavaps_cnt = 0;

    ie++; /*to get ie len*/
    ie_len = *ie;
    ie += 4; /*to get extender ie content*/
    extender_ie_type = *ie++;
    extender_ie_content = *ie++;
    ie_len -= 5;
    if (ftype == IEEE80211_FRAME_TYPE_ASSOCREQ) {
        ni->is_extender_client = 1;
        GLOBAL_IC_LOCK_BH(ic_list);
        ic_list->num_rptr_clients++;
        GLOBAL_IC_UNLOCK_BH(ic_list);
    } else if (ftype == IEEE80211_FRAME_TYPE_ASSOCRESP) {
        ic->ic_extender_connection = 1;
        if (ic_list->num_stavaps_up == 0) {
            apvaps_cnt = *ie++;
            stavaps_cnt = *ie++;
            mac_list = (u_int8_t *)ic_list->preferred_list_stavap;
            for (i=0; ((i < apvaps_cnt)&&(ie_len > 0)); i++) {
                GLOBAL_IC_LOCK_BH(ic_list);
                IEEE80211_ADDR_COPY((mac_list+(i*IEEE80211_ADDR_LEN)), ie);
                GLOBAL_IC_UNLOCK_BH(ic_list);
                qdf_print("Preferred mac[%d]:%s",i,ether_sprintf(mac_list+(i*IEEE80211_ADDR_LEN)));
                ie += IEEE80211_ADDR_LEN;
                ie_len -= IEEE80211_ADDR_LEN;
            }
            mac_list = (u_int8_t *)ic_list->denied_list_apvap;
            for (i=0; ((i < stavaps_cnt)&&(ie_len > 0)); i++) {
                GLOBAL_IC_LOCK_BH(ic_list);
                IEEE80211_ADDR_COPY((mac_list+(i*IEEE80211_ADDR_LEN)), ie);
                GLOBAL_IC_UNLOCK_BH(ic_list);
                qdf_print("Denied mac[%d]:%s",i,ether_sprintf(mac_list+(i*IEEE80211_ADDR_LEN)));
                ie += IEEE80211_ADDR_LEN;
                ie_len -= IEEE80211_ADDR_LEN;
            }

            GLOBAL_IC_LOCK_BH(ic_list);
            ic_list->ap_preferrence = 2;
            GLOBAL_IC_UNLOCK_BH(ic_list);
        }
        if ((extender_ie_content & ROOTAP_ACCESS_MASK) == ROOTAP_ACCESS_MASK) {
            /*If connecting RE has RootAP access*/
            ic->ic_extender_connection = 2;
        }
    }

    return;
}

#endif

uint8_t *ieee80211_mgmt_add_chan_switch_ie(uint8_t *frm, struct ieee80211_node *ni,
                uint8_t subtype, uint8_t chanchange_tbtt)
{

        struct ieee80211vap *vap = ni->ni_vap;
        struct ieee80211_ath_channelswitch_ie *csaie = (struct ieee80211_ath_channelswitch_ie*)frm;
        struct ieee80211com *ic = ni->ni_ic;
        struct ieee80211_extendedchannelswitch_ie *ecsa_ie = NULL;
        struct ieee80211_max_chan_switch_time_ie *mcst_ie = NULL;
        uint8_t csa_ecsa_mcst_len = IEEE80211_CHANSWITCHANN_BYTES;
        uint8_t csmode = IEEE80211_CSA_MODE_STA_TX_ALLOWED;
        /* the length of csa, ecsa and max chan switch time(mcst) ies
         * is represented by csa_ecsa_mcst_len, but it is initialised
         * with csa length and based on the presence of ecsa and mcst
         * the length is increased.
         */

        if (vap->iv_csmode == IEEE80211_CSA_MODE_AUTO) {

            /* No user preference for csmode. Use default behavior.
             * If chan swith is triggered because of radar found
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
             * Use user preference
             */
            csmode = vap->iv_csmode;
        }

        /* CSA Action frame format:
         * [1] Category code - Spectrum management (0).
         * [1] Action code - Channel Switch Announcement (4).
         * [TLV] Channel Switch Announcement IE.
         * [TLV] Secondary Channel Offset IE.
         * [TLV] Wide Bandwidth IE.
         */

        /* Check if ecsa IE has to be added.
         * If yes, adjust csa_ecsa_mcst_len to include CSA IE len
         * and ECSA IE len.
         */
        if (vap->iv_enable_ecsaie) {
            ecsa_ie = (struct ieee80211_extendedchannelswitch_ie *)(frm + csa_ecsa_mcst_len);
            csa_ecsa_mcst_len += IEEE80211_EXTCHANSWITCHANN_BYTES;
        }

        /* Check if max chan switch time IE(mcst IE) has to be added.
         * If yes, adjust csa_ecsa_mcst_len to include CSA IE len,
         * ECSA IE len and mcst IE len.
         */
        if (vap->iv_enable_max_ch_sw_time_ie) {
            mcst_ie = (struct ieee80211_max_chan_switch_time_ie *)(frm + csa_ecsa_mcst_len);
            csa_ecsa_mcst_len += IEEE80211_MAXCHANSWITCHTIME_BYTES;
        }

        csaie->ie = IEEE80211_ELEMID_CHANSWITCHANN;
        csaie->len = 3; /* fixed len */
        csaie->switchmode = csmode;
        csaie->newchannel = ic->ic_chanchange_chan;
        csaie->tbttcount = chanchange_tbtt;

        if (ecsa_ie) {
            ecsa_ie->ie = IEEE80211_ELEMID_EXTCHANSWITCHANN;
            ecsa_ie->len = 4; /* fixed len */
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
            ecsa_ie->tbttcount = chanchange_tbtt;
        }

        if (mcst_ie) {
            ieee80211_add_max_chan_switch_time(vap, (uint8_t *)mcst_ie);
        }

        frm += csa_ecsa_mcst_len;

        if (((IEEE80211_IS_CHAN_11N(vap->iv_bsschan) ||
                        IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
                        IEEE80211_IS_CHAN_11AX(vap->iv_bsschan)) &&
                    (ic->ic_chanchange_secoffset)) && ic->ic_sec_offsetie) {

            /* Add secondary channel offset element */
            struct ieee80211_ie_sec_chan_offset *sec_chan_offset_ie = NULL;

            sec_chan_offset_ie = (struct ieee80211_ie_sec_chan_offset *)frm;
            sec_chan_offset_ie->elem_id = IEEE80211_ELEMID_SECCHANOFFSET;

            /* Element has only one octet of info */
            sec_chan_offset_ie->len = 1;
            sec_chan_offset_ie->sec_chan_offset = ic->ic_chanchange_secoffset;
            frm += IEEE80211_SEC_CHAN_OFFSET_BYTES;
        }

        if ((IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
                IEEE80211_IS_CHAN_11AXA(vap->iv_bsschan)) &&
                ieee80211vap_vhtallowed(vap)
                && (ic->ic_chanchange_channel != NULL)) {
            /* Adding channel switch wrapper element */
            frm = ieee80211_add_chan_switch_wrp(frm, ni, ic, subtype,
                    /* When switching to new country by sending ECSA IE,
                     * new country IE should be also be added.
                     * As of now we dont support switching to new country
                     * without bringing down vaps so new country IE is not
                     * required.
                     */
                    (/*ecsa_ie ? IEEE80211_VHT_EXTCH_SWITCH :*/
                     !IEEE80211_VHT_EXTCH_SWITCH));
    }
    return frm;
}
