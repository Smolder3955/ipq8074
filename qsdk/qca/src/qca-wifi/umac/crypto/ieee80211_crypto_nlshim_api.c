/*
 *
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if UMAC_SUPPORT_CFG80211

#include <osif_private.h>
#include <ieee80211_defines.h>
#include <ieee80211_var.h>
#include <ol_if_athvar.h>
#include <ol_ath_ucfg.h>
#include <ieee80211_ucfg.h>
#include "ieee80211_crypto_nlshim_api.h"


/**
 * wlan_cfg80211_get_ie_ptr
 * @ies_ptr:  Pointer to beacon buffer
 * @length: Length of Beacon buffer
 * @eid: Element ID of IE to return
 */

uint8_t *wlan_cfg80211_get_ie_ptr(struct ieee80211vap *vap,const uint8_t *ies_ptr, int tail_len,
	uint8_t eid)
{
    int length = tail_len;
    uint8_t *ptr = (uint8_t *)ies_ptr;
    uint8_t elem_id, elem_len;

    while (length >= 2) {
        elem_id = ptr[0];
        elem_len = ptr[1];
        length -= 2;
        if (elem_len > length) {
            qdf_print("Invalid IEs eid = %d elem_len=%d left=%d",
                    eid, elem_len, length);
            return NULL;
        }
        if (!eid && !is_hostie(ptr,IE_LEN(ptr))) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CFG80211, "%s: Adding elementID: %d \n",
                              __func__, elem_id);
            wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_BEACON,ptr,IE_LEN(ptr), HOSTAPD_IE);
            wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_PROBERESP,ptr,IE_LEN(ptr), HOSTAPD_IE);
            vap->appie_buf_updated = 1;
        } else if  (elem_id == eid) {
            return ptr;
        }

        length -= elem_len;
        ptr += (elem_len + 2);
    }
    return NULL;
}

int  wlan_cfg80211_crypto_setting(struct net_device *dev,
                                  struct cfg80211_crypto_settings *params,
                                  enum nl80211_auth_type  auth_type)
{
    int error = -EOPNOTSUPP;
    u_int8_t i =0;
    unsigned int args[10] = {0,0,0};
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int32_t value=0;
    u_int32_t cipher=0;
    u_int32_t authmode=0;
    u_int32_t key_mgmt=0;
    u_int8_t ret_val = 0;

    /* group key cipher  IEEE80211_PARAM_MCASTCIPHER */

    switch (params->cipher_group) {
        case WLAN_CIPHER_SUITE_WEP40:
        case WLAN_CIPHER_SUITE_WEP104:
            cipher = IEEE80211_CIPHER_WEP ;
            break;

        case  WLAN_CIPHER_SUITE_TKIP:
            cipher = IEEE80211_CIPHER_TKIP ;
            break;

        case WLAN_CIPHER_SUITE_GCMP:
            cipher = IEEE80211_CIPHER_AES_GCM;
            break;

        case WLAN_CIPHER_SUITE_GCMP_256:
            cipher = IEEE80211_CIPHER_AES_GCM_256;
            break;

        case WLAN_CIPHER_SUITE_CCMP:
            cipher = IEEE80211_CIPHER_AES_CCM;
            break;

        case WLAN_CIPHER_SUITE_CCMP_256:
            cipher = IEEE80211_CIPHER_AES_CCM_256;
            break;

        default :
            cipher  = IEEE80211_CIPHER_NONE;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
            authmode = IEEE80211_AUTH_OPEN;
#else
            authmode = (1 << WLAN_CRYPTO_AUTH_OPEN);
#endif
            break;

    }


#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    if ( auth_type == NL80211_AUTHTYPE_SHARED_KEY )
        authmode = IEEE80211_AUTH_SHARED ;
    if ( auth_type == NL80211_AUTHTYPE_AUTOMATIC )
        authmode = IEEE80211_AUTH_AUTO;
    if ( auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM )
        authmode = IEEE80211_AUTH_OPEN;
#else
    if ( auth_type == NL80211_AUTHTYPE_SHARED_KEY )
        authmode = (1 << WLAN_CRYPTO_AUTH_SHARED);
    if ( auth_type == NL80211_AUTHTYPE_AUTOMATIC )
        authmode = ((1 << WLAN_CRYPTO_AUTH_SHARED) | (1 << WLAN_CRYPTO_AUTH_OPEN));
    if ( auth_type == NL80211_AUTHTYPE_OPEN_SYSTEM )
        authmode = (1 << WLAN_CRYPTO_AUTH_OPEN);
#endif

    if ( cipher  == IEEE80211_CIPHER_WEP) {
        /* key length is done only for specific ciphers */
        if (params->control_port_no_encrypt)
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
            authmode = IEEE80211_AUTH_8021X;
#else
            authmode = (1 << WLAN_CRYPTO_AUTH_8021X);
#endif
        else {
            ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_UCASTCIPHERS,1 << cipher,(char*)args);
            ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_MCASTCIPHER,cipher,(char*)args);
            value = (params->cipher_group == WLAN_CIPHER_SUITE_WEP104 ? 13 : 5);
            error = ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_MCASTKEYLEN,value,(char*)args);
            if (error) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,"Unable to set group key length to %u\n", value);
            }
        }
     } else {
        ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_MCASTCIPHER,cipher,(char*)args);
     }

    value = 0;

/* pairwise key ciphers  IEEE80211_PARAM_UCASTCIPHERS*/
    for( i = 0;i < params->n_ciphers_pairwise ; i++) {
        switch(params->ciphers_pairwise[i]) {
            case WLAN_CIPHER_SUITE_TKIP:
                value |= 1<<IEEE80211_CIPHER_TKIP;
                break;

	    case  WLAN_CIPHER_SUITE_CCMP:
                value |= 1<<IEEE80211_CIPHER_AES_CCM;
                break;

            case WLAN_CIPHER_SUITE_CCMP_256:
                value |= 1<<IEEE80211_CIPHER_AES_CCM_256;
                break;

            case WLAN_CIPHER_SUITE_GCMP:
                value |= 1<<IEEE80211_CIPHER_AES_GCM;
                break;  

            case  WLAN_CIPHER_SUITE_GCMP_256:
                value |= 1<<IEEE80211_CIPHER_AES_GCM_256;
                break;

            case WLAN_CIPHER_SUITE_WEP104:
            case WLAN_CIPHER_SUITE_WEP40:
                value |= 1<<IEEE80211_CIPHER_WEP;
                break;
            default:
                break;
        }
    }
    if(params->n_ciphers_pairwise)
        ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_UCASTCIPHERS,value,(char*)args);
    else
        ret_val = 1;

/*key management algorithms IEEE80211_PARAM_KEYMGTALGS */

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    if(params->n_akm_suites ) {
/* AKM suite selectors */
        switch(params->akm_suites[0] ) {
            case WLAN_AKM_SUITE_PSK:
                 key_mgmt = WPA_ASE_8021X_PSK;
                 authmode = IEEE80211_AUTH_WPA ;
                 break;
            case WLAN_AKM_SUITE_FT_8021X:
                 key_mgmt = WPA_ASE_FT_IEEE8021X;
                 break;
            case WLAN_AKM_SUITE_FT_PSK:
                 key_mgmt = WPA_ASE_FT_PSK;
                 break;
            case WLAN_AKM_SUITE_8021X_SHA256:
                 key_mgmt = WPA_ASE_SHA256_IEEE8021X;
                 break;
            case WLAN_AKM_SUITE_PSK_SHA256:
                 key_mgmt = WPA_ASE_SHA256_PSK;
                 break;
            case WLAN_AKM_SUITE_8021X_SUITE_B:
            case WLAN_AKM_SUITE_8021X_SUITE_B_192:
            case WLAN_AKM_SUITE_CCKM:
            case WLAN_AKM_SUITE_OSEN:
                 key_mgmt = WPA_ASE_8021X_UNSPEC;
                 break;
            default:
                key_mgmt = WPA_ASE_NONE;
                break;
        }
        ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_KEYMGTALGS,key_mgmt,(char*)args);
    }

/* enable WPA  IEEE80211_PARAM_WPA */

    if((cipher  != IEEE80211_CIPHER_WEP) && (cipher != IEEE80211_CIPHER_NONE))
        ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_WPA,params->wpa_versions,(char*)args);
    else
        ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_AUTHMODE,authmode,(char*)args);
#else
    if (params->n_akm_suites)
        authmode = 0;

    for (i = 0; i < params->n_akm_suites; i++) {
    /* AKM suite selectors */
        switch(params->akm_suites[i] ) {
            case WLAN_AKM_SUITE_PSK:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_PSK);
                 /* set the authmode type to WPA if we use mixed or WPA1 security type
                    and is not required for WPA2 security types */
                 if ((params->wpa_versions == 1) || (params->wpa_versions == 3))
                     authmode |= (1 << WLAN_CRYPTO_AUTH_WPA);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_RSNA);
                 break;
            case WLAN_AKM_SUITE_FT_8021X:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_8021X);
                 break;
            case WLAN_AKM_SUITE_FT_PSK:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_FT_PSK);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_RSNA);
                 break;
            case WLAN_AKM_SUITE_8021X_SHA256:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_8021X);
                 break;
            case WLAN_AKM_SUITE_PSK_SHA256:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_PSK_SHA256);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_RSNA);
                 break;
            case WLAN_AKM_SUITE_8021X_SUITE_B:
            case WLAN_AKM_SUITE_8021X_SUITE_B_192:
            case WLAN_AKM_SUITE_CCKM:
            case WLAN_AKM_SUITE_OSEN:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_IEEE8021X);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_8021X);
                 break;
            case WLAN_AKM_SUITE_OWE:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_OWE);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_RSNA);
                 break;
            case WLAN_AKM_SUITE_SAE:
            case WLAN_AKM_SUITE_FT_OVER_SAE:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_SAE);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_RSNA);
                 break;
            case WLAN_AKM_SUITE_DPP:
                 key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_DPP);
                 authmode |= (1 << WLAN_CRYPTO_AUTH_RSNA);
                 break;
            default:
                key_mgmt |= (1 << WLAN_CRYPTO_KEY_MGMT_NONE);
                break;
        }
    }

    if (params->n_akm_suites)
        wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_KEY_MGMT, key_mgmt);

    wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE, authmode);

#endif /* WLAN_CONV_CRYPTO_SUPPORTED */

  return ret_val;
}



int wlan_set_beacon_ies(struct  net_device *dev, struct cfg80211_beacon_data *beacon)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int8_t *rsn_ie = NULL;
    u_int8_t *wpa_ie = NULL;
    u_int8_t *ptr = (u_int8_t *)beacon->tail;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    struct wlan_crypto_params crypto_params = {0} ;
    int status = IEEE80211_STATUS_SUCCESS;
    unsigned int args[2] = {0,0};
#endif


    /* Remove previous hostapd IE for dynamic security mode to open mode */
    if (vap->vie_handle) {
         wlan_mlme_app_ie_delete_id(vap->vie_handle,IEEE80211_FRAME_TYPE_BEACON,HOSTAPD_IE);
         wlan_mlme_app_ie_delete_id(vap->vie_handle,IEEE80211_FRAME_TYPE_PROBERESP,HOSTAPD_IE);
         wlan_mlme_app_ie_delete_id(vap->vie_handle,IEEE80211_FRAME_TYPE_ASSOCRESP,HOSTAPD_IE);
         vap->appie_buf_updated = 1;
     }

    if(beacon->tail_len) {
        wlan_cfg80211_get_ie_ptr(vap,beacon->tail, beacon->tail_len,0);
    }
    /*
     * beacon->tail IEs has generic IEs like
     * HTCAP, VHTCAP, WMM etc. for these generic IEs driver will generate
     * them and add to beacons.
     * beacon_ies from Hostpad contains
     * WLAN_EID_EXT_CAPAB
     * WLAN_EID_INTERWORKING
     * WLAN_EID_ADV_PROTO
     * WLAN_EID_ROAMING_CONSORTIUM
     * fst_ies, wps_beacon_ie, hostapd_eid_hs20_indication, vendor_elements
     * we will append these IEs in app_ies.
     *
     */
    if (beacon->beacon_ies_len) {
        wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_BEACON,beacon->beacon_ies,beacon->beacon_ies_len, HOSTAPD_IE);
        if (beacon->beacon_ies_len && iswpsoui(beacon->beacon_ies)) {
            wlan_set_param(vap,IEEE80211_WPS_MODE,1);
        } else if(( beacon->beacon_ies_len == 0) && vap->iv_wps_mode) {
            wlan_set_param(vap,IEEE80211_WPS_MODE,0);
        }
        vap->appie_buf_updated = 1;
    }

    if (beacon->proberesp_ies_len) {
        wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_PROBERESP,beacon->proberesp_ies,beacon->proberesp_ies_len, HOSTAPD_IE);
        vap->appie_buf_updated = 1;
    }

    if (beacon->assocresp_ies_len) {
        wlan_mlme_app_ie_set_check(vap, IEEE80211_FRAME_TYPE_ASSOCRESP,beacon->assocresp_ies,beacon->assocresp_ies_len, HOSTAPD_IE);
        vap->appie_buf_updated = 1;
    }

    while (((ptr + 1) < (u_int8_t *)beacon->tail + beacon->tail_len) && (ptr + ptr[1] + 1 < (u_int8_t *)beacon->tail + beacon->tail_len)) {
        if (ptr[0] == WLAN_ELEMID_RSN && ptr[1] >= 20 ){
            rsn_ie = ptr;
        } else if (ptr[0] == WLAN_ELEMID_VENDOR && iswpaoui(ptr) ){
            wpa_ie = ptr;
        }
        ptr += ptr[1] + 2;
    }

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if (wpa_ie != NULL)
    {
        status = wlan_crypto_wpaie_check((struct wlan_crypto_params *)&crypto_params, wpa_ie);
        if (status != QDF_STATUS_SUCCESS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,"wpa ie unavailable\n");
        }
    }

    if (rsn_ie != NULL)
    {
        status = wlan_crypto_rsnie_check((struct wlan_crypto_params *)&crypto_params, rsn_ie);
        if (status != QDF_STATUS_SUCCESS) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,"rsn ie unavailable\n");
        }
    }

    if ( (wpa_ie || rsn_ie ) &&((crypto_params.rsn_caps & RSN_CAP_MFP_ENABLED) || (crypto_params.rsn_caps & RSN_CAP_MFP_REQUIRED))
            && status == QDF_STATUS_SUCCESS) {
	ieee80211_ucfg_setparam(vap,IEEE80211_PARAM_RSNCAPS, crypto_params.rsn_caps ,(char*)args);
   }
#endif

    return 0;
}

/**
 * wlan_cfg80211_setcurity_init() -  Intialize the security parameters
 * @dev: Pointer to netdev
 * @params: Pointer to start ap configuration parameters
 * struct cfg80211_ap_settings - AP configuration
 * Return: zero for success non-zero for failure
 */
int wlan_cfg80211_security_init(struct cfg80211_ap_settings *params,struct net_device *dev)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    wlan_cfg80211_crypto_setting(dev,&(params->crypto), params->auth_type);

     /*set privacy */
    wlan_set_param(vap,IEEE80211_FEATURE_PRIVACY,params->privacy);

    wlan_set_beacon_ies(dev,&(params->beacon));
    return 0;
}


/**
 * wlan_cfg80211_add_key() - cfg80211 add key handler function
 * @wiphy: Pointer to wiphy structure.
 * @dev: Pointer to net_device structure.
 * @key_index: key index
 * @pairwise: pairwise
 * @mac_addr: mac address
 * @params: key parameters
 *
 * Return: 0 for success, error number on failure.
 */

int wlan_cfg80211_add_key(struct wiphy *wiphy,
        struct net_device *ndev,
        u8 key_index, bool pairwise,
        const u8 *mac_addr,
        struct key_params *params)
{
    int error = -EOPNOTSUPP;
    osif_dev *osifp = ath_netdev_priv(ndev);
    wlan_if_t vap = osifp->os_if;
    ieee80211_keyval key_val;
    u_int8_t keydata[IEEE80211_KEYBUF_SIZE];
    struct ieee80211req_key wk;

    memset(&key_val,0, sizeof(ieee80211_keyval));

    memset(&wk, 0, sizeof(wk));
 
    switch (params->cipher) {
        case WLAN_CIPHER_SUITE_WEP40:
        case WLAN_CIPHER_SUITE_WEP104:
            key_val.keytype  = IEEE80211_CIPHER_WEP ;
            break;

        case  WLAN_CIPHER_SUITE_TKIP:
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                     "%s: init. TKIP PN to 0x%x%x, tsc=0x%x%x.\n", __func__,
                     (u_int32_t)(key_val.keyrsc>>32), (u_int32_t)(key_val.keyrsc),
                     (u_int32_t)(key_val.keytsc>>32), (u_int32_t)(key_val.keytsc));

            key_val.keytype  = IEEE80211_CIPHER_TKIP;
            break;

        case WLAN_CIPHER_SUITE_CCMP:
            key_val.keytype  = IEEE80211_CIPHER_AES_CCM;
            break;

        case WLAN_CIPHER_SUITE_GCMP:
            key_val.keytype  = IEEE80211_CIPHER_AES_GCM;
            break;

        case WLAN_CIPHER_SUITE_CCMP_256:
            key_val.keytype  = IEEE80211_CIPHER_AES_CCM_256;
            break;

        case WLAN_CIPHER_SUITE_GCMP_256:
            key_val.keytype  = IEEE80211_CIPHER_AES_GCM_256;
            break;

        case WLAN_CIPHER_SUITE_AES_CMAC:
            key_val.keytype  = IEEE80211_CIPHER_AES_CMAC;
            break;

        case WLAN_CIPHER_SUITE_BIP_CMAC_256:
            key_val.keytype  = IEEE80211_CIPHER_AES_CMAC_256;
            break;

        case WLAN_CIPHER_SUITE_BIP_GMAC_128:
            key_val.keytype  = IEEE80211_CIPHER_AES_GMAC;
            break;

        case WLAN_CIPHER_SUITE_BIP_GMAC_256:
            key_val.keytype  = IEEE80211_CIPHER_AES_GMAC_256;
            break;

        default :
            key_val.keytype  = IEEE80211_CIPHER_NONE;
            break;
    }

    if (osifp->os_opmode == IEEE80211_M_STA ) {
          if (params->seq_len > MAX_SEQ_LEN) {
           return -EINVAL;
         }
         wk.ik_flags = IEEE80211_KEY_RECV;
         if(pairwise )
                  wk.ik_flags |= IEEE80211_KEY_XMIT;
#ifndef IEEE80211_KEY_GROUP
#define IEEE80211_KEY_GROUP 0x04
#endif

  /* Need to remove wk ? . No more useful ?*/

         if(mac_addr &&  !is_broadcast_ether_addr(mac_addr)) {
             if ( key_val.keytype != IEEE80211_CIPHER_WEP && key_index && !pairwise) {
                 osifp->m_count = 1;
                 osifp->mciphers[0] = key_val.keytype;
                 wlan_set_mcast_ciphers(vap,osifp->mciphers,osifp->m_count);
                  wk.ik_flags |= IEEE80211_KEY_GROUP;
                  wk.ik_keyix = key_index;
             } else  {
                osifp->u_count = 1;
                osifp->uciphers[0] = key_val.keytype;
                wlan_set_ucast_ciphers(vap,osifp->uciphers,osifp->u_count);
                 wk.ik_keyix = key_index == 0 ? IEEE80211_KEYIX_NONE :  key_index;
             }
            key_val.macaddr = (u_int8_t *) mac_addr ;
         } else {
                 osifp->m_count = 1;
                 osifp->mciphers[0] = key_val.keytype;
                 wlan_set_mcast_ciphers(vap,osifp->mciphers,osifp->m_count);
                 key_val.macaddr = (u_int8_t *)ieee80211broadcastaddr ;
                 wk.ik_flags |= IEEE80211_KEY_GROUP;
                 wk.ik_keyix = key_index;
         } 

          if (wk.ik_keyix != IEEE80211_KEYIX_NONE && pairwise) {
            wk.ik_flags |= IEEE80211_KEY_DEFAULT; 
          } 
 

         if (  wk.ik_flags & IEEE80211_KEY_XMIT )
          key_val.keydir = IEEE80211_KEY_DIR_BOTH ;
         else 
          key_val.keydir = IEEE80211_KEY_DIR_RX ;


         if( params->seq) {
		   memcpy(&key_val.keyrsc, params->seq, params->seq_len);
         }
         key_val.keyindex = wk.ik_keyix ;
    } else {

        if (!mac_addr)
        {
            key_val.macaddr = (u_int8_t *)ieee80211broadcastaddr ;
            key_val.keyindex = key_index;
            if (pairwise) {
                wk.ik_flags |= IEEE80211_KEY_DEFAULT;
            }
        } else {
            key_val.macaddr = (u_int8_t *) mac_addr ;
            key_val.keyindex = IEEE80211_KEYIX_NONE ;
        }

        key_val.keydir = IEEE80211_KEY_DIR_BOTH;
    }

    key_val.keyrsc  = 0;
    key_val.keytsc  = 0;
    if (params->seq) {
        uint64_t v = 0xFFU;
        if (params->seq_len == 8)
            key_val.keyrsc =
                (((uint64_t)params->seq[0] << 56) & (v << 56)) |
                (((uint64_t)params->seq[1] << 48) & (v << 48)) |
                (((uint64_t)params->seq[2] << 40) & (v << 40)) |
                (((uint64_t)params->seq[3] << 32) & (v << 32)) |
                ((params->seq[4] << 24) & (v << 24)) |
                ((params->seq[5] << 16) & (v << 16)) |
                ((params->seq[6] <<  8) & (v << 8)) |
                (params->seq[7]         & v);

        if (params->seq_len == 6)
            key_val.keyrsc =
                (((uint64_t)params->seq[0] << 40) & (v << 40)) |
                (((uint64_t)params->seq[1] << 32) & (v << 32)) |
                ((params->seq[2] << 24) & (v << 24)) |
                ((params->seq[3] << 16) & (v << 16)) |
                ((params->seq[4] <<  8) & (v << 8)) |
                (params->seq[5]         & v);
    }

    key_val.keylen  =  params->key_len;

    if (key_val.keylen == 0) {
        /* zero length keys will only set default key id if flags are set*/
        if ((!pairwise) && (key_val.keyindex!= IEEE80211_KEYIX_NONE)) {
            /* default xmit key */
            wlan_set_default_keyid(vap, key_val.keyindex);
            return 0;
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,"%s: Zero length key\n", __func__);
        return -EINVAL;
    }


     if (key_val.keyindex == IEEE80211_KEYIX_NONE)
    {

        if (osifp->os_opmode == IEEE80211_M_STA ||
            osifp->os_opmode == IEEE80211_M_P2P_CLIENT)
        {
            int i=0;
            for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
                if (mac_addr[i] != 0) {
                    break;
                }
            }
            if (i == IEEE80211_ADDR_LEN) {
                key_val.macaddr = (u_int8_t *)ieee80211broadcastaddr ;
            }
        }
     } else {
            if ((key_val.keyindex >= IEEE80211_WEP_NKID)
                && (key_val.keytype != IEEE80211_CIPHER_AES_CMAC)
                && (key_val.keytype != IEEE80211_CIPHER_AES_CMAC_256)
                && (key_val.keytype != IEEE80211_CIPHER_AES_GMAC)
                && (key_val.keytype != IEEE80211_CIPHER_AES_GMAC_256)) {
                return -EINVAL;
            }

            if (!IEEE80211_IS_MULTICAST(key_val.macaddr) &&
                ((key_val.keytype == IEEE80211_CIPHER_TKIP) ||
                (key_val.keytype == IEEE80211_CIPHER_AES_CCM) ||
                (key_val.keytype == IEEE80211_CIPHER_AES_CCM_256) ||
                (key_val.keytype == IEEE80211_CIPHER_AES_GCM) ||
                (key_val.keytype == IEEE80211_CIPHER_AES_GCM_256) )) {
                key_val.keyindex = IEEE80211_KEYIX_NONE;
            }
    }


    if (key_val.keylen > IEEE80211_KEYBUF_SIZE)
        key_val.keylen  = IEEE80211_KEYBUF_SIZE;
    key_val.rxmic_offset = IEEE80211_KEYBUF_SIZE + 8;
    key_val.txmic_offset =  IEEE80211_KEYBUF_SIZE;

    memcpy(keydata, params->key, params->key_len);

    key_val.keydata = keydata;

    if(key_val.keytype == IEEE80211_CIPHER_TKIP) {
        key_val.rxmic_offset = TKIP_RXMIC_OFFSET;
        key_val.txmic_offset = TKIP_TXMIC_OFFSET;
    }


   if(key_val.keytype == IEEE80211_CIPHER_WEP
            && vap->iv_wep_keycache) {
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 0);
        /* only static wep keys will allocate index 0-3 in keycache
         *if we are using 802.1x with WEP then it should go to else part
         *to mandate this new iwpriv commnad wepkaycache is used
         */
    }
    else {
        wlan_set_param(vap, IEEE80211_WEP_MBSSID, 1);
        /* allow keys to allocate anywhere in key cache */
    }

    error = wlan_set_key(vap,key_val.keyindex,&key_val);

    wlan_set_param(vap, IEEE80211_WEP_MBSSID, 0);  /* put it back to default */

    if ( (pairwise) &&(key_val.keyindex != IEEE80211_KEYIX_NONE) &&
        (key_val.keytype != IEEE80211_CIPHER_AES_CMAC) && (key_val.keytype != IEEE80211_CIPHER_AES_CMAC_256) &&
        (key_val.keytype != IEEE80211_CIPHER_AES_GMAC) && (key_val.keytype != IEEE80211_CIPHER_AES_GMAC_256)) {
        /* default xmit key */
        wlan_set_default_keyid(vap,key_val.keyindex);
    }

    return 0;
}

/**
 * wlan_cfg80211_get_key() - cfg80211 get key handler function
 * @wiphy: Pointer to wiphy structure.
 * @ndev: Pointer to net_device structure.
 * @key_index: key index
 * @pairwise: pairwise
 * @mac_addr: mac address
 * @cookie : cookie information
 * @params: key parameters
 *
 * Return: 0 for success, error number on failure.
 */
 
int wlan_cfg80211_get_key(struct wiphy *wiphy,
        struct net_device *ndev,
        u8 key_index, bool pairwise,
        const u8 *mac_addr, void *cookie,
        void (*callback)(void *cookie,
            struct key_params *)
        )
{
    struct key_params params;
    ieee80211_keyval kval;
    u_int8_t keydata[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
    osif_dev *osifp = ath_netdev_priv(ndev);
    u_int8_t  bssid[IEEE80211_ADDR_LEN];
    wlan_if_t vap = osifp->os_if;

    memset(&params, 0, sizeof(params));

    if(!mac_addr)
    {
         memset(bssid, 0xFF, IEEE80211_ADDR_LEN);
    } else {
         memcpy(bssid,mac_addr, IEEE80211_ADDR_LEN);
    }

    if (key_index != IEEE80211_KEYIX_NONE)
    {
        if (key_index >= IEEE80211_WEP_NKID)
            return -EINVAL;
    }

    kval.keydata = keydata;

    if (wlan_get_key(vap,key_index,bssid, &kval, IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE) != 0)
    {
        return -EINVAL;
    }

    params.cipher =  kval.keytype;
    params.key_len = kval.keylen;
    params.seq_len = 0;
    params.seq = NULL;
    params.key = kval.keydata;
    callback(cookie, &params);
    return 0;
}

/**
 * wlan_del_key() - cfg80211 delete key handler function
 * @wiphy: Pointer to wiphy structure.
 * @dev: Pointer to net_device structure.
 * @key_index: key index
 * @pairwise: pairwise
 * @mac_addr: mac address
 *
 * Return: 0 for success, error number on failure.
 */
int wlan_cfg80211_del_key(struct wiphy *wiphy,
        struct net_device *dev,
        u8 key_index,
        bool pairwise, const u8 *mac_addr)
{
    osif_dev *osifp = ath_netdev_priv(dev);
    u_int8_t  bssid[IEEE80211_ADDR_LEN];
    wlan_if_t vap = NULL;

    if (osifp == NULL) {
        return 0;
    }
    vap = osifp->os_if;

    if (vap == NULL) {
        return 0;
    }

    if( vap->iv_ic->recovery_in_progress) {
        qdf_print("%s: FW Crash on vap %d ...check", __func__, vap->iv_unit);
        return 0;
    }

    if(!mac_addr)
    {
         memset(bssid, 0xFF, IEEE80211_ADDR_LEN);
    } else {
         IEEE80211_ADDR_COPY(bssid,mac_addr);
    }
    if (key_index == KEYIX_INVALID) {
        ieee80211_del_key(vap,osifp->authmode,IEEE80211_KEYIX_NONE,bssid);
    } else {
        ieee80211_del_key(vap,osifp->authmode,key_index,bssid);
    }
    return 0;
}

/**
 * wlan_cfg80211_set_default_key : Set default key
 * @wiphy: pointer to wiphy structure
 * @ndev: pointer to net_device
 * @key_index: key_index
 * @unicast : unicast key
 * @multicast : multicast key
 *
 * Return; 0 on success, error number otherwise
 */
int wlan_cfg80211_set_default_key(struct wiphy *wiphy,
        struct net_device *ndev,
        u8 key_index,
        bool unicast, bool multicast)
{
    osif_dev *osifp = ath_netdev_priv(ndev);
    wlan_if_t vap = osifp->os_if;

    /* default xmit key */
    wlan_set_default_keyid(vap,key_index);

    return 0;
}

#endif
