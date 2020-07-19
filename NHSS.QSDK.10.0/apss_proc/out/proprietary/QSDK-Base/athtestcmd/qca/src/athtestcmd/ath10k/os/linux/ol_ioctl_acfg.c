/*
 * Copyright (c) 2010, Qualcomm Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */

#include <asm/uaccess.h>

#include <osif_private.h>
#include <wlan_opts.h>

#include "ieee80211_var.h"
#include "ol_if_athvar.h"
#include "if_athioctl.h"
#include "ol_params.h"
#include "asf_amem.h"
#include "dbglog_host.h"
#include "ol_regdomain.h"
#include "ol_txrx_dbg.h"

#define __ACFG_PHYMODE_STRINGS__
#include <adf_os_types_pvt.h>
#include <acfg_drv_if.h>
#include <acfg_api_types.h>
#include <acfg_drv_dev.h>
#include <acfg_drv_cfg.h>
#include <acfg_drv_cmd.h>
#include <acfg_drv_event.h>

/*
** "split" of config param values, since they are all combined
** into the same table.  This value is a "shift" value for ATH parameters
*/
#define OL_ATH_PARAM_SHIFT     0x1000
#define OL_SPECIAL_PARAM_SHIFT 0x2000
#define OL_MGMT_RETRY_LIMIT_MIN (1)
#define OL_MGMT_RETRY_LIMIT_MAX (15)

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
    OL_SPECIAL_PARAM_ENABLE_SHPREAMBLE,
    OL_SPECIAL_PARAM_ENABLE_SHSLOT,
    OL_SPECIAL_PARAM_RADIO_MGMT_RETRY_LIMIT,
    OL_SPECIAL_PARAM_SENS_LEVEL,
    OL_SPECIAL_PARAM_TX_POWER_5G,
    OL_SPECIAL_PARAM_TX_POWER_2G,
    OL_SPECIAL_PARAM_CCA_THRESHOLD,
};

extern  int wmi_unified_pdev_set_param(wmi_unified_t wmi_handle,
        WMI_PDEV_PARAM param_id, u_int32_t param_value);

static void
ol_acfg_convert_to_acfgprofile (struct ieee80211_profile *profile,
                                    acfg_radio_vap_info_t *acfg_profile)
{
    a_uint8_t i, kid = 0;

	strncpy(acfg_profile->radio_name, profile->radio_name, IFNAMSIZ);
	acfg_profile->chan = profile->channel;
	acfg_profile->freq = profile->freq;
	acfg_profile->country_code = profile->cc;
	memcpy(acfg_profile->radio_mac, profile->radio_mac, ACFG_MACADDR_LEN);
    for (i = 0; i < profile->num_vaps; i++) {
        strncpy(acfg_profile->vap_info[i].vap_name,
                                    profile->vap_profile[i].name,
                                    IFNAMSIZ);
		memcpy(acfg_profile->vap_info[i].vap_mac, 
				profile->vap_profile[i].vap_mac, 
				ACFG_MACADDR_LEN);
		acfg_profile->vap_info[i].phymode = 
									profile->vap_profile[i].phymode;		
		switch (profile->vap_profile[i].sec_method) {
			case IEEE80211_AUTH_NONE:
			case IEEE80211_AUTH_OPEN:
				if(profile->vap_profile[i].cipher & (1 << IEEE80211_CIPHER_WEP))

				{
					acfg_profile->vap_info[i].cipher = 
										ACFG_WLAN_PROFILE_CIPHER_METH_WEP;
					acfg_profile->vap_info[i].sec_method = 
											ACFG_WLAN_PROFILE_SEC_METH_OPEN;
				} else {
					acfg_profile->vap_info[i].cipher = 
										ACFG_WLAN_PROFILE_CIPHER_METH_NONE;
					acfg_profile->vap_info[i].sec_method = 
											ACFG_WLAN_PROFILE_SEC_METH_OPEN;
				}
				break;
			case IEEE80211_AUTH_AUTO:
				acfg_profile->vap_info[i].sec_method = 
											ACFG_WLAN_PROFILE_SEC_METH_AUTO;
				if(profile->vap_profile[i].cipher & 
						(1 << IEEE80211_CIPHER_WEP))

				{
					acfg_profile->vap_info[i].cipher = 
										ACFG_WLAN_PROFILE_CIPHER_METH_WEP;
				}
				break;
			case IEEE80211_AUTH_SHARED:
				if(profile->vap_profile[i].cipher & 
						(1 << IEEE80211_CIPHER_WEP))
				{
					acfg_profile->vap_info[i].cipher = 
										ACFG_WLAN_PROFILE_CIPHER_METH_WEP;
				}
				acfg_profile->vap_info[i].sec_method = 
											ACFG_WLAN_PROFILE_SEC_METH_SHARED;
				break;	
			case IEEE80211_AUTH_WPA:
				acfg_profile->vap_info[i].sec_method = 
											ACFG_WLAN_PROFILE_SEC_METH_WPA;
				break;
			case IEEE80211_AUTH_RSNA:
				acfg_profile->vap_info[i].sec_method = 
											ACFG_WLAN_PROFILE_SEC_METH_WPA2;
				break;
		}
		if (profile->vap_profile[i].cipher & (1 << IEEE80211_CIPHER_TKIP)) {
			acfg_profile->vap_info[i].cipher |= 
										ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
		}
		if ((profile->vap_profile[i].cipher & (1 << IEEE80211_CIPHER_AES_OCB)) 
			 ||(profile->vap_profile[i].cipher & 
											(1 <<IEEE80211_CIPHER_AES_CCM))) 
		{
			acfg_profile->vap_info[i].cipher |= 
										ACFG_WLAN_PROFILE_CIPHER_METH_AES;
		}
		for (kid = 0; kid < 4; kid++) {
			if (profile->vap_profile[i].wep_key_len[kid]) {
        		memcpy(acfg_profile->vap_info[i].wep_key,
                	    profile->vap_profile[i].wep_key[kid], 
						profile->vap_profile[i].wep_key_len[kid]);
				acfg_profile->vap_info[i].wep_key_idx = kid;
				acfg_profile->vap_info[i].wep_key_len =
									profile->vap_profile[i].wep_key_len[kid];
			}
		}
    }
    acfg_profile->num_vaps = profile->num_vaps;
}

int
ol_acfg_vap_create(void *ctx, a_uint16_t cmdid,
               a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ieee80211_clone_params cp;
    acfg_vapinfo_t    *ptr;
    acfg_os_req_t   *req = NULL;
    struct ifreq ifr;
    int status;
	struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);


    req = (acfg_os_req_t *) buffer;
    ptr     = &req->data.vap_info;

    memset(&ifr, 0, sizeof(ifr));
    memset(&cp , 0, sizeof(cp));

    cp.icp_opmode = ptr->icp_opmode;
    cp.icp_flags  = ptr->icp_flags;
    cp.icp_vapid  = ptr->icp_vapid;

    memcpy(&cp.icp_name[0], &ptr->icp_name[0], ACFG_MAX_IFNAME);
    ifr.ifr_data = (void *) &cp;

    status  = osif_ioctl_create_vap(dev, &ifr, cp, scn->sc_osdev);

    return status;
}

int
ol_acfg_set_wifi_param(void *ctx, a_uint16_t cmdid,
                   a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_param_req_t        *ptr;
    acfg_os_req_t   *req = NULL;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    int param, value, retval = 0;
    struct ieee80211com     *ic = &scn->sc_ic;
    struct ieee80211_channel *chan = ieee80211_get_current_channel(ic);
    u_int16_t freq = ieee80211_chan2freq(ic, chan);
    u_int32_t flags = chan->ic_flags;
    u_int8_t cfreq2 = chan->ic_vhtop_ch_freq_seg2;
    bool restart_vaps = FALSE;

    req = (acfg_os_req_t *) buffer;
    ptr  = &req->data.param_req;

    param = ptr->param;
    value = ptr->val;

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
        case OL_SPECIAL_PARAM_REGDOMAIN:
            /*
             * After this command, the channel list would change.
             * must set ic_curchan properly.
             */
            retval = wlan_set_regdomain(ic, value);

            chan = ieee80211_find_channel(ic, freq, cfreq2, flags);
            if (chan) {
                ic->ic_curchan = chan;
                ic->ic_set_channel(ic);
            } else 
                printk("Current channel not supported in new RD, must set a new channel\n");
            break; 
        case OL_SPECIAL_PARAM_ENABLE_SHPREAMBLE:
            if (value) {
                scn->sc_ic.ic_caps |= IEEE80211_C_SHPREAMBLE;
            } else {
                scn->sc_ic.ic_caps &= ~IEEE80211_C_SHPREAMBLE;
            }
            break;
        case OL_SPECIAL_PARAM_ENABLE_SHSLOT:
            if (value)
                ieee80211_set_shortslottime(&scn->sc_ic, 1);
            else
                ieee80211_set_shortslottime(&scn->sc_ic, 0);
            break;
        case OL_SPECIAL_PARAM_RADIO_MGMT_RETRY_LIMIT:
            /* mgmt retry limit 1-15 */
            if( value < OL_MGMT_RETRY_LIMIT_MIN || value > OL_MGMT_RETRY_LIMIT_MAX ){
                printk("mgmt retry limit invalid, should be in (1-15)\n");
                retval = -EINVAL;
            }else{
                retval = ic->ic_set_mgmt_retry_limit(ic, value);
            }
            break;
        case OL_SPECIAL_PARAM_SENS_LEVEL:
            printk("%s[%d] PARAM_SENS_LEVEL \n", __func__,__LINE__);
            retval = wmi_unified_pdev_set_param(scn->wmi_handle,
                WMI_PDEV_PARAM_SENSITIVITY_LEVEL, value);
            break;
        case OL_SPECIAL_PARAM_TX_POWER_5G:
            printk("%s[%d] OL_SPECIAL_PARAM_TX_POWER_5G \n", __func__,__LINE__);
            retval = wmi_unified_pdev_set_param(scn->wmi_handle,
                WMI_PDEV_PARAM_SIGNED_TXPOWER_5G, value);
            break;
        case OL_SPECIAL_PARAM_TX_POWER_2G:
            printk("%s[%d] OL_SPECIAL_PARAM_TX_POWER_2G \n", __func__,__LINE__);
            retval = wmi_unified_pdev_set_param(scn->wmi_handle,
                WMI_PDEV_PARAM_SIGNED_TXPOWER_2G, value);
            break;
        case OL_SPECIAL_PARAM_CCA_THRESHOLD:
            printk("%s[%d] PARAM_CCA_THRESHOLD \n", __func__,__LINE__);
            retval = wmi_unified_pdev_set_param(scn->wmi_handle,
                WMI_PDEV_PARAM_CCA_THRESHOLD, value);
            break;
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

int
ol_acfg_get_wifi_param(void *ctx, a_uint16_t cmdid,
                   a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_param_req_t        *ptr;
    acfg_os_req_t   *req = NULL;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    int param, *val, retval = 0;

    req = (acfg_os_req_t *) buffer;
    ptr  = &req->data.param_req;
    
    val = &ptr->val;	
    param = ptr->param;

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
        if (ol_ath_get_config_param(scn, (ol_ath_param_t)param, (void *)val))
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
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_SHPREAMBLE) ) {
            val[0] = (scn->sc_ic.ic_caps & IEEE80211_C_SHPREAMBLE) != 0;
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_SHSLOT) ) {
            val[0] = IEEE80211_IS_SHSLOT_ENABLED(&scn->sc_ic) ? 1 : 0;
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_RADIO_MGMT_RETRY_LIMIT) ) {
            val[0] = ic->ic_get_mgmt_retry_limit(ic);
        } else {
            retval = -EOPNOTSUPP;
        }
    }
    else
    {
        if ( ol_hal_get_config_param(scn, (ol_hal_param_t)param, (void *)val))
        {
            retval = -EOPNOTSUPP;
        }
    }

    return (retval);
}

int
ol_acfg_set_reg (void *ctx, a_uint16_t cmdid,
            a_uint8_t *buffer, a_int32_t Length)
{
    return -1;
}

int
ol_acfg_get_reg (void *ctx, a_uint16_t cmdid,
            a_uint8_t *buffer, a_int32_t Length)
{
    return -1;
}

#ifdef ATH_TX99_DIAG
extern u_int8_t tx99_ioctl(struct net_device *dev, struct ol_ath_softc_net80211 *sc, int cmd, void *addr);
#endif
extern int utf_unified_ioctl (struct ol_ath_softc_net80211 *scn, struct ifreq *ifr);

int
ol_acfg_tx99tool (void *ctx, a_uint16_t cmdid,
               a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    int error = 0;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_data = (void *)&req->data;

#if defined(ATH_TX99_DIAG) && (!defined(ATH_PERF_PWR_OFFLOAD))
    error = tx99_ioctl(dev, ATH_DEV_TO_SC(scn->sc_dev), SIOCIOCTLTX99, ifr.ifr_data);
#else
    error = utf_unified_ioctl(scn, &ifr);
#endif
    return error;
}

int
ol_acfg_set_hwaddr (void *ctx, a_uint16_t cmdid,
               a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macaddr_t *paddr;
    struct sockaddr mac;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    int status = ENOTSUPP;
    
    paddr = &req->data.macaddr;
    memcpy(mac.sa_data, paddr, ACFG_MACADDR_LEN);
    
    rtnl_lock(); 

    if (dev->netdev_ops->ndo_set_mac_address)
        status = dev->netdev_ops->ndo_set_mac_address(dev, &mac);

    rtnl_unlock();
    
    return status;
}

int
ol_acfg_get_ath_stats (void *ctx, a_uint16_t cmdid,
               a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct ol_stats stats;
    acfg_ath_stats_t *asc;
    int error = 0;

    asc = &req->data.ath_stats;

    if((dev->flags & IFF_UP) == 0)
    {
        return -ENXIO;
    }
    if(error || asc->size == 0 || asc->address == NULL) 
    {
        error = -EFAULT;
    }
    else 
    {
        stats.txrx_stats_level = ol_txrx_stats_publish(scn->pdev_txrx_handle, &stats.txrx_stats);
        ol_get_wlan_dbg_stats(scn, &stats.stats);
        ol_get_radio_stats(scn, &stats.interface_stats);
        if (_copy_to_user(asc->address, &stats, sizeof(struct ol_stats)))
            error = -EFAULT;
        else
            error = 0;
    }
    return error;
}

int
ol_acfg_clr_ath_stats (void *ctx, a_uint16_t cmdid,
               a_uint8_t *buffer, a_int32_t Length)
{
    /* This IOCTL is currently not supported */
    return 0;
}

int
ol_acfg_get_profile (void *ctx, a_uint16_t cmdid,
               a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = NULL;
    acfg_radio_vap_info_t *ptr;
    struct ieee80211_profile *profile;
        int error = 0;

    req = (acfg_os_req_t *) buffer;
    ptr     = &req->data.radio_vap_info;
    memset(ptr, 0, sizeof (acfg_radio_vap_info_t));
    profile = (struct ieee80211_profile *)kmalloc(
                            sizeof (struct ieee80211_profile), GFP_KERNEL);
    if (profile == NULL) {
        return -ENOMEM;
    }
    OS_MEMSET(profile, 0, sizeof (struct ieee80211_profile));
    error = osif_ioctl_get_vap_info(dev, profile);
    ol_acfg_convert_to_acfgprofile(profile, ptr);
    if (profile != NULL) {
        kfree(profile);
        profile = NULL;
    }

    return 0;
}

int ol_ath_get_nf_dbr_dbm_info(void *ctx, a_uint16_t cmdid,
                                  a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);

    if(wmi_unified_nf_dbr_dbm_info_get(scn->wmi_handle)) {
        printk("%s:Unable to send request to get NF dbr dbm info\n", __func__);
        return -1;
    }
    return 0;
}

int ol_ath_get_packet_power_info(void *ctx, a_uint16_t cmdid,
                                  a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;

    acfg_packet_power_param_t *ptr;
    ptr = &req->data.packet_power_param;

    if(wmi_unified_packet_power_info_get(scn->wmi_handle, ptr->rate_flags, ptr->nss, ptr->preamble, ptr->hw_rate)) {
        printk("%s:Unable to send request to get packet power info\n", __func__);
        return -1;
    }
    return 0;
}

extern int ath_ioctl_phyerr(struct ol_ath_softc_net80211 *scn, struct ath_diag *ad);

int
ol_acfg_phyerr (void *ctx, a_uint16_t cmdid,
               a_uint8_t *buffer, a_int32_t Length)
{
    int error = 0;
#if defined(ATH_SUPPORT_DFS) || defined(ATH_SUPPORT_SPECTRAL)
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    acfg_os_req_t   *req = NULL;

    req = (acfg_os_req_t *) buffer;

    if (!capable(CAP_NET_ADMIN)) {
        error = -EPERM;
    } else {
        error = ath_ioctl_phyerr(scn, (struct ath_diag *)&req->data.ath_diag);
    }
#endif
    return error;
}


/* GPIO config routine */
int ol_ath_gpio_config(void *ctx, a_uint16_t cmdid,
                                  a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);

    acfg_gpio_config_t *ptr;
    ptr = &req->data.gpio_config;

    printk("%s[%d] gpio_num %d input %d pull_type %d, intr_mode %d\n", __func__,__LINE__, ptr->gpio_num, ptr->input, ptr->pull_type, ptr->intr_mode);
    if(wmi_unified_gpio_config(scn->wmi_handle, ptr->gpio_num, ptr->input, ptr->pull_type, ptr->intr_mode)) {
        printk("%s:Unable to config GPIO\n", __func__);
        return -1;
    }
    return 0;
}

/* GPIO set routine */
int ol_ath_gpio_set(void *ctx, a_uint16_t cmdid,
                                  a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);

    acfg_gpio_set_t *ptr;
    ptr = &req->data.gpio_set;

    printk("%s[%d] gpio_num %d set %d\n", __func__,__LINE__, ptr->gpio_num, ptr->set);
    if(wmi_unified_gpio_output(scn->wmi_handle, ptr->gpio_num, ptr->set)) {
        printk("%s:Unable to set GPIO\n", __func__);
        return -1;
    }
    return 0;
}

/* Set CTL power table */
int ol_ath_acfg_ctl_set(void *ctx, a_uint16_t cmdid,
                                  a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    wmi_buf_t buf;
    wmi_pdev_set_ctl_table_cmd *cmd;
    u_int16_t len;
    acfg_ctl_table_t *ptr;

    ptr = &req->data.ctl_table;
    printk("%s[%d] Mode %d CTL table length %d\n", __func__,__LINE__, ptr->band, ptr->len);

    len = sizeof(wmi_pdev_set_ctl_table_cmd);
    len += roundup(ptr->len, sizeof(A_UINT32)) - sizeof(A_UINT32); /* already 4 bytes in cmd structure */
    len += sizeof(a_int32_t); // band flag is the first 4-byte element of CTL table array
    printk("wmi buf len = %d\n", len);
    buf = wmi_buf_alloc(scn->wmi_handle, len);
    if (!buf) {
        adf_os_print("%s:wmi_buf_alloc failed\n", __FUNCTION__);
        return -1;
    }
    cmd = (wmi_pdev_set_ctl_table_cmd *)wmi_buf_data(buf);   
    cmd->ctl_len = ptr->len+sizeof(a_int32_t);
    
    // Copy 4 bytes for band and then ptr->len bytes for CTL data
    OL_IF_MSG_COPY_CHAR_ARRAY(&cmd->ctl_info[0], &ptr->band, ptr->len+sizeof(a_int32_t));

    if(wmi_unified_cmd_send(scn->wmi_handle, buf, ptr->len+sizeof(a_int32_t), WMI_PDEV_SET_CTL_TABLE_CMDID)) {
        printk("%s:Unable to set CTL table\n", __func__);
        return -1;
    }
    return 0;
}

/*
 * @brief set basic & supported rates in beacon,
 * and use lowest basic rate as mgmt mgmt/bcast/mcast rates by default.
 * target_rates: an array of supported rates with bit7 set for basic rates.
 */
static int
ol_acfg_set_vap_op_support_rates(wlan_if_t vap, struct ieee80211_rateset *target_rs)
{
    struct ieee80211_node *bss_ni = vap->iv_bss;
    enum ieee80211_phymode mode = wlan_get_desired_phymode(vap);
    struct ieee80211_rateset *bss_rs = &(bss_ni->ni_rates);
    struct ieee80211_rateset *op_rs = &(vap->iv_op_rates[mode]);
    struct ieee80211_rateset *ic_supported_rs = &(vap->iv_ic->ic_sup_rates[mode]);
    a_uint8_t num_of_rates, num_of_basic_rates, i, j, rate_found=0;
    a_uint8_t basic_rates[IEEE80211_RATE_MAXSIZE];
    a_int32_t retv=0, min_rate=0;

    num_of_rates = target_rs->rs_nrates;
    if(num_of_rates > ACFG_MAX_RATE_SIZE){
        num_of_rates = ACFG_MAX_RATE_SIZE;
    }

    /* Check if the new rates are supported by the IC */
    for (i=0; i < num_of_rates; i++) {
        rate_found = 0;
        for (j=0; j < (ic_supported_rs->rs_nrates); j++) {
            if((target_rs->rs_rates[i]&IEEE80211_RATE_VAL) == 
                        (ic_supported_rs->rs_rates[j]&IEEE80211_RATE_VAL)){
                rate_found  = 1;
                break;
            }
        }
        if(!rate_found){
            printk("Error: rate %d not supported in phymode %s !\n",
                    (target_rs->rs_rates[i]&IEEE80211_RATE_VAL)/2,ieee80211_phymode_name[mode]);
            return EINVAL;
        }
    }

    /* Update BSS rates and VAP supported rates with the new rates */
    for (i=0; i < num_of_rates; i++) {
        bss_rs->rs_rates[i] = target_rs->rs_rates[i];
        op_rs->rs_rates[i] = target_rs->rs_rates[i];
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "rate %d (%d kbps) added\n",
                                target_rs->rs_rates[i], (target_rs->rs_rates[i]&IEEE80211_RATE_VAL)*1000/2);
    }
    bss_rs->rs_nrates = num_of_rates;
    op_rs->rs_nrates = num_of_rates;

    if (IEEE80211_VAP_IS_PUREG_ENABLED(vap)) {
        /*For pureg mode, all 11g rates are marked as Basic*/
        ieee80211_setpuregbasicrates(op_rs);
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, 
                      "%s Mark flag so that new rates will be used in next beacon update\n",__func__);
    vap->iv_flags_ext2 |= IEEE80211_FEXT2_BR_UPDATE;

    /* Find all basic rates */
    num_of_basic_rates = 0;
    for (i=0; i < bss_rs->rs_nrates; i++) {
        if(bss_rs->rs_rates[i] & IEEE80211_RATE_BASIC){
            basic_rates[num_of_basic_rates] = bss_rs->rs_rates[i];
            num_of_basic_rates++;
        }
    }
    if(!num_of_basic_rates){
        printk("%s: Error, no basic rates set. \n",__FUNCTION__);
        return EINVAL;
    }

    /* Find lowest basic rate */
    min_rate = basic_rates[0];
    for (i=0; i < num_of_basic_rates; i++) {
        if ( min_rate > basic_rates[i] ) {
            min_rate = basic_rates[i];
        }
    }

   /*
    * wlan_set_param supports actual rate in unit of kbps
    * min: 1000 kbps max: 300000 kbps
    */
    min_rate = ((min_rate&IEEE80211_RATE_VAL)*1000)/2;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, 
                    "%s Set default mgmt/bcast/mcast rates to %d Kbps\n",__func__,min_rate);

    /* Use lowest basic rate as mgmt mgmt/bcast/mcast rates by default */
    retv = wlan_set_param(vap, IEEE80211_MGMT_RATE, min_rate);
    if(retv){
        return retv;
    }

    retv = wlan_set_param(vap, IEEE80211_BCAST_RATE, min_rate);
    if(retv){
        return retv;
    }

    retv = wlan_set_param(vap, IEEE80211_MCAST_RATE, min_rate);
    if(retv){
        return retv;
    }

    return retv;
}

/**
 * @brief set basic & supported rates in beacon,
 * and use lowest basic rate as mgmt mgmt/bcast/mcast rates by default.
 */
static int
ol_acfg_set_op_support_rates(void *ctx, a_uint16_t cmdid,
             a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_rateset_t  *target_rs=&(req->data.rs);
    wlan_if_t tmpvap=NULL;
    a_int32_t retv = -EINVAL;

    TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
        if(!tmpvap){
            return retv;
        }
        retv = ol_acfg_set_vap_op_support_rates(tmpvap, (struct ieee80211_rateset *)target_rs);
        if(retv){
            printk("%s: Set VAP basic rates failed, retv=%d\n",__FUNCTION__,retv);
            return retv;
        }
    }

    return 0;
}

/**
 * @brief get physically supported rates of the specified phymode for the radio
 */
static int
ol_acfg_get_radio_supported_rates(void *ctx, a_uint16_t cmdid,
             a_uint8_t *buffer, a_int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_rateset_phymode_t *rs_phymode = &(req->data.rs_phymode);
    acfg_rateset_t  *target_rs= &(rs_phymode->rs);
    struct ieee80211_rateset *rs = NULL;
    enum ieee80211_phymode mode = rs_phymode->phymode;
    a_uint8_t j=0;

    if (!IEEE80211_SUPPORT_PHY_MODE(ic, mode)) {
        printk("%s: The radio doesn't support this phymode: %d\n",__FUNCTION__,mode);
        return -EINVAL;
    }

    rs = &ic->ic_sup_rates[mode];
    if(!rs){
        return -EINVAL;
    }

    for (j=0; j<(rs->rs_nrates); j++) {
        target_rs->rs_rates[j] = rs->rs_rates[j];
    }
    target_rs->rs_nrates = rs->rs_nrates;

    return 0;
}

acfg_dispatch_entry_t ol_acfgdispatchentry[] =
{
    { 0,                         ACFG_REQ_FIRST_WIFI        , 0 }, //--> 0
    { ol_acfg_vap_create,        ACFG_REQ_CREATE_VAP        , 0 },
    { ol_acfg_set_wifi_param,    ACFG_REQ_SET_RADIO_PARAM   , 0 },
    { ol_acfg_get_wifi_param,    ACFG_REQ_GET_RADIO_PARAM   , 0 },
    { ol_acfg_set_reg,           ACFG_REQ_SET_REG           , 0 },
    { ol_acfg_get_reg,           ACFG_REQ_GET_REG           , 0 },
    { ol_acfg_tx99tool,          ACFG_REQ_TX99TOOL          , 0 },
    { ol_acfg_set_hwaddr,        ACFG_REQ_SET_HW_ADDR       , 0 },
    { ol_acfg_get_ath_stats,     ACFG_REQ_GET_ATHSTATS      , 0 },
    { ol_acfg_clr_ath_stats,     ACFG_REQ_CLR_ATHSTATS      , 0 },
    { ol_acfg_get_profile,       ACFG_REQ_GET_PROFILE       , 0 },
    { ol_acfg_phyerr,            ACFG_REQ_PHYERR            , 0 },
    { ol_ath_gpio_config,        ACFG_REQ_GPIO_CONFIG       , 0 },
    { ol_ath_gpio_set,           ACFG_REQ_GPIO_SET          , 0 },
    { ol_ath_acfg_ctl_set,       ACFG_REQ_SET_CTL_TABLE     , 0 },
    { ol_acfg_set_op_support_rates,   ACFG_REQ_SET_OP_SUPPORT_RATES   , 0 },
    { ol_acfg_get_radio_supported_rates, ACFG_REQ_GET_RADIO_SUPPORTED_RATES   , 0 },
    { ol_ath_get_nf_dbr_dbm_info, ACFG_REQ_GET_NF_DBR_DBM_INFO   , 0 },
    { ol_ath_get_packet_power_info, ACFG_REQ_GET_PACKET_POWER_INFO   , 0 },
    { 0,                         ACFG_REQ_LAST_WIFI         , 0 },
};

int
ol_acfg_handle_wifi_ioctl(struct net_device *dev, void *data, int *isvap_ioctl)
{
    acfg_os_req_t   *req = NULL;
    uint32_t    req_len = sizeof(struct acfg_os_req);
    int32_t status = 0;
    uint32_t i;
    bool cmd_found = false;

    req = kzalloc(req_len, GFP_KERNEL);
    if(!req){
        printk("%s: mem alloc failed\n",__FUNCTION__);
        return ENOMEM;
    }
    if(copy_from_user(req, data, req_len)){
        printk("%s: copy_from_user failed\n",__FUNCTION__);
        goto done;
    }
    /* Iterate the dispatch table to find the handler
     * for the command received from the user */
    if(req->cmd > ACFG_REQ_FIRST_WIFI && req->cmd < ACFG_REQ_LAST_WIFI) 
    {
        *isvap_ioctl = 0;
        for (i = 0; i < sizeof(ol_acfgdispatchentry)/sizeof(ol_acfgdispatchentry[0]); i++)
        { 
            if (ol_acfgdispatchentry[i].cmdid == req->cmd) {
                cmd_found = true;
                break;
            }
        }
        if ((cmd_found == false) || (ol_acfgdispatchentry[i].cmd_handler == NULL)) {
            status = -1;
            printk("OL ACFG ioctl CMD %d failed\n", req->cmd);
            goto done;
        }
        status = ol_acfgdispatchentry[i].cmd_handler(dev,
                        req->cmd, (uint8_t *)req, req_len);
        if(copy_to_user(data, req, req_len)) {
            printk("OL copy_to_user error\n");
            status = -1;
    	}
    }
    else
    {
        *isvap_ioctl = 1;
        printk("%s: req->cmd=%d not valid for radio interface, it's for VAP\n",
                  __func__,req->cmd);
    }
done:
    kfree(req);
    return status;
}

int
ol_acfg_handle_ioctl(struct net_device *dev, void *data)
{
    int isvap_ioctl = 0;
    int status = 0;

    status = ol_acfg_handle_wifi_ioctl(dev, data, &isvap_ioctl);
    if(isvap_ioctl)
    {
        return -EINVAL;
    }

    return status;
}

