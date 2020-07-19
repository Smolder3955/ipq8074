/*
 * Copyright (c) 2014-2015,2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2014-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <linux/config.h>
#endif
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
#include <ieee80211_ioctl.h>
#include "ieee80211_rateset.h"
#include "ieee80211_vi_dbg.h"
#if ATH_SUPPORT_IBSS_DFS
#include <ieee80211_regdmn.h>
#endif
#include "ieee80211_power_priv.h"
#include "../vendor/generic/ioctl/ioctl_vendor_generic.h"
#include <ol_if_athvar.h>
#include "if_athvar.h"
#include "if_athproto.h"

#define __ACFG_PHYMODE_STRINGS__
#include <i_qdf_types.h>
#include <acfg_api_types.h>
#include <acfg_drv_cfg.h>
#include <acfg_drv_cmd.h>
#include <acfg_drv_event.h>

#include "ieee80211.h"
#include  "ieee80211_acfg.h"
#include  "ieee80211_ioctl_acfg.h"
#include <linux/time.h>

#include "ath_ucfg.h"
#include "ieee80211_ucfg.h"
#include "osdep.h"
#if QCA_AIRTIME_FAIRNESS
#include <wlan_atf_ucfg_api.h>
#endif
#include <ieee80211_mlme_dfs_dispatcher.h>
#if WLAN_SUPPORT_SPLITMAC
#include "wlan_splitmac.h"
#endif
#include <wlan_offchan_txrx_api.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif
#include <wlan_utility.h>

#define MAX_OFFCHAN_WAIT 50
unsigned long prev_jiffies = 0;

#ifdef __ACFG_PHYMODE_STRINGS__

/**
 *  * @brief String representations of phy modes
 *   *        in capital letters.
 *    */
static const uint8_t *phymode_strings[] = {
    [ACFG_PHYMODE_AUTO]             = (uint8_t *)"AUTO",
    [ACFG_PHYMODE_11A]              = (uint8_t *)"11A",
    [ACFG_PHYMODE_11B]              = (uint8_t *)"11B",
    [ACFG_PHYMODE_11G]              = (uint8_t *)"11G" ,
    [ACFG_PHYMODE_FH]               = (uint8_t *)"FH" ,
    [ACFG_PHYMODE_TURBO_A]               = (uint8_t *)"TA" ,
    [ACFG_PHYMODE_TURBO_G]               = (uint8_t *)"TG" ,
    [ACFG_PHYMODE_11NA_HT20]        = (uint8_t *)"11NAHT20" ,
    [ACFG_PHYMODE_11NG_HT20]        = (uint8_t *)"11NGHT20" ,
    [ACFG_PHYMODE_11NA_HT40PLUS]    = (uint8_t *)"11NAHT40PLUS" ,
    [ACFG_PHYMODE_11NA_HT40MINUS]   = (uint8_t *)"11NAHT40MINUS" ,
    [ACFG_PHYMODE_11NG_HT40PLUS]    = (uint8_t *)"11NGHT40PLUS" ,
    [ACFG_PHYMODE_11NG_HT40MINUS]   = (uint8_t *)"11NGHT40MINUS" ,
    [ACFG_PHYMODE_11NG_HT40]   = (uint8_t *)"11NGHT40" ,
    [ACFG_PHYMODE_11NA_HT40]   = (uint8_t *)"11NAHT40" ,
    [ACFG_PHYMODE_11AC_VHT20]  = (uint8_t *)"11ACVHT20" ,
    [ACFG_PHYMODE_11AC_VHT40PLUS]  = (uint8_t *)"11ACVHT40PLUS" ,
    [ACFG_PHYMODE_11AC_VHT40MINUS]  = (uint8_t *)"11ACVHT40MINUS" ,
    [ACFG_PHYMODE_11AC_VHT40]  = (uint8_t *)"11ACVHT40" ,
    [ACFG_PHYMODE_11AC_VHT80]  = (uint8_t *)"11ACVHT80" ,
    [ACFG_PHYMODE_11AC_VHT160]  = (uint8_t *)"11ACVHT160" ,
    [ACFG_PHYMODE_11AC_VHT80_80]  = (uint8_t *)"11ACVHT80_80" ,
    [ACFG_PHYMODE_11AXA_HE20] = (uint8_t *) "11AHE20" ,
    [ACFG_PHYMODE_11AXG_HE20] = (uint8_t *) "11GHE20" ,
    [ACFG_PHYMODE_11AXA_HE40PLUS] = (uint8_t *) "11AHE40PLUS" ,
    [ACFG_PHYMODE_11AXA_HE40MINUS] = (uint8_t *) "11AHE40MINUS" ,
    [ACFG_PHYMODE_11AXG_HE40PLUS] = (uint8_t *) "11GHE40PLUS" ,
    [ACFG_PHYMODE_11AXG_HE40MINUS]   = (uint8_t *) "11GHE40MINUS" ,
    [ACFG_PHYMODE_11AXA_HE40] = (uint8_t *) "11AHE40" ,
    [ACFG_PHYMODE_11AXG_HE40] = (uint8_t *) "11GHE40" ,
    [ACFG_PHYMODE_11AXA_HE80] = (uint8_t *) "11AHE80" ,
    [ACFG_PHYMODE_11AXA_HE160] = (uint8_t *) "11AHE160" ,
    [ACFG_PHYMODE_11AXA_HE80_80] = (uint8_t *) "11AHE80_80" ,

    [ACFG_PHYMODE_INVALID]          = NULL ,
};
#endif //__ACFG_PHYMODE_STRINGS__


#define RATE_KBPS_TO_MBPS(r) ((r) / 1024)

static const int legacy_rate_idx[][2] = {
    {1000, 0x1b},
    {2000, 0x1a},
    {5500, 0x19},
    {6000, 0xb},
    {9000, 0xf},
    {11000, 0x18},
    {12000, 0xa},
    {18000, 0xe},
    {24000, 0x9},
    {36000, 0xd},
    {48000, 0x8},
    {54000, 0xc},
};


#define MAX_DIAGNOSTICS_WAIT    1000
#define WAIT_15_SEC             15
#define WAIT_1_HR               3600

#define HT_RATE_CODE            0x80
#define HT_RC_2_MCS(_rc)        ((_rc) & 0x7f)
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)

#define AR600P_GET_HW_RATECODE_PREAM(_rcode)     (((_rcode) >> 6) & 0x3)
#define AR600P_GET_HW_RATECODE_NSS(_rcode)       (((_rcode) >> 4) & 0x3)
#define AR600P_GET_HW_RATECODE_RATE(_rcode)      (((_rcode) >> 0) & 0xF)
#define IS_HW_RATECODE_HT(_rc)  (((_rc) & 0xc0) == 0x80)
#define IS_HW_RATECODE_VHT(_rc) (((_rc) & 0xc0) == 0xc0)
#define VHT_2_HT_MCS(_mcs, _nss) (((_nss) * 8) + (_mcs))

#define WHAL_RC_FLAG_SGI        0x08
#define WHAL_RC_FLAG_40MHZ      0x20
#define WHAL_RC_FLAG_80MHZ      0x40
#define WHAL_RC_FLAG_160MHZ     0x80

extern int ieee80211_rate_is_valid_basic(struct ieee80211vap *, u_int32_t);

#if ACFG_NETLINK_TX
void acfg_homechan_tx_send(struct ieee80211vap *vap, acfg_netlink_pvt_t *acfg_nl);
#endif
static struct ieee80211vap *ieee80211_acfg_find_vap(uint8_t *vap_name, struct ieee80211com **ic_ptr);

#ifdef QCA_PARTNER_PLATFORM
extern int wlan_pltfrm_set_param(wlan_if_t vaphandle, u_int32_t val);
extern int wlan_pltfrm_get_param(wlan_if_t vaphandle);
#endif

void
acfg_convert_to_acfgprofile (struct ieee80211_profile *profile,
                                    acfg_radio_vap_info_t *acfg_profile)
{
    uint8_t i, kid = 0;

	strlcpy(acfg_profile->radio_name, profile->radio_name, IFNAMSIZ);
	acfg_profile->chan = profile->channel;
	acfg_profile->freq = profile->freq;
	acfg_profile->country_code = profile->cc;
	memcpy(acfg_profile->radio_mac, profile->radio_mac, ACFG_MACADDR_LEN);
    for (i = 0; i < profile->num_vaps; i++) {
        strlcpy(acfg_profile->vap_info[i].vap_name,
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

static int
acfg_acfg2ieee(uint16_t param)
{
    uint16_t value;
    switch (param) {
        case ACFG_PARAM_VAP_SHORT_GI:
            value = IEEE80211_PARAM_SHORT_GI;
            break;
        case ACFG_PARAM_VAP_AMPDU:
            value = IEEE80211_PARAM_AMPDU;
            break;
        case ACFG_PARAM_AUTHMODE:
            value = IEEE80211_PARAM_AUTHMODE;
            break;
        case ACFG_PARAM_MACCMD:
            value = IEEE80211_PARAM_MACCMD;
            break;
        case ACFG_PARAM_BEACON_INTERVAL:
            value = IEEE80211_PARAM_BEACON_INTERVAL;
            break;
        case ACFG_PARAM_DTIM_PERIOD:
            value = IEEE80211_PARAM_DTIM_PERIOD;
            break;
        case ACFG_PARAM_PUREG:
            value = IEEE80211_PARAM_PUREG;
            break;
        case ACFG_PARAM_WDS:
            value = IEEE80211_PARAM_WDS;
            break;
		case ACFG_PARAM_UAPSD:
            value = IEEE80211_PARAM_UAPSDINFO;
            break;
        case ACFG_PARAM_11N_RATE:
            value = IEEE80211_PARAM_11N_RATE;
            break;
        case ACFG_PARAM_11N_RETRIES:
            value = IEEE80211_PARAM_11N_RETRIES;
            break;
        case ACFG_PARAM_DBG_LVL:
            value = IEEE80211_PARAM_DBG_LVL;
            break;
        case ACFG_PARAM_STA_FORWARD:
            value = IEEE80211_PARAM_STA_FORWARD;
            break;
        case ACFG_PARAM_PUREN:
            value = IEEE80211_PARAM_PUREN;
            break;
        case ACFG_PARAM_PURE11AC:
            value = IEEE80211_PARAM_PURE11AC;
            break;
        case ACFG_PARAM_NO_EDGE_CH:
            value = IEEE80211_PARAM_NO_EDGE_CH;
            break;
        case ACFG_PARAM_WPS:
            value = IEEE80211_PARAM_WPS;
            break;
        case ACFG_PARAM_EXTAP:
            value = IEEE80211_PARAM_EXTAP;
            break;
        case ACFG_PARAM_SW_WOW:
            value = IEEE80211_PARAM_SW_WOW;
            break;
        case ACFG_PARAM_HIDE_SSID:
            value = IEEE80211_PARAM_HIDESSID;
            break;
        case ACFG_PARAM_DOTH:
            value = IEEE80211_PARAM_DOTH;
            break;
        case ACFG_PARAM_COEXT_DISABLE:
            value = IEEE80211_PARAM_COEXT_DISABLE;
            break;
        case ACFG_PARAM_APBRIDGE:
            value = IEEE80211_PARAM_APBRIDGE;
            break;
        case ACFG_PARAM_AMPDU:
            value = IEEE80211_PARAM_AMPDU;
            break;
        case ACFG_PARAM_ROAMING:
            value = IEEE80211_PARAM_ROAMING;
            break;
        case ACFG_PARAM_AUTO_ASSOC:
            value = IEEE80211_PARAM_AUTO_ASSOC;
            break;
		case ACFG_PARAM_UCASTCIPHERS:
			value = IEEE80211_PARAM_UCASTCIPHERS;
			break;
        case ACFG_PARAM_CHANBW:
            value = IEEE80211_PARAM_CHANBW;
            break;
        case ACFG_PARAM_BURST:
            value = IEEE80211_PARAM_BURST;
            break;
        case ACFG_PARAM_AMSDU:
            value = IEEE80211_PARAM_AMSDU;
            break;
        case ACFG_PARAM_MAXSTA:
            value = IEEE80211_PARAM_MAXSTA;
            break;
        case ACFG_PARAM_SETADDBAOPER:
            value = IEEE80211_PARAM_SETADDBAOPER;
            break;
        case ACFG_PARAM_OPMODE_NOTIFY:
            value = IEEE80211_PARAM_OPMODE_NOTIFY;
            break;
        case ACFG_PARAM_WEP_TKIP_HT:
            value = IEEE80211_PARAM_WEP_TKIP_HT;
            break;
        case ACFG_PARAM_CWM_ENABLE:
            value = IEEE80211_PARAM_CWM_ENABLE;
            break;
        case ACFG_PARAM_MAX_AMPDU:
            value = IEEE80211_PARAM_MAX_AMPDU;
            break;
        case ACFG_PARAM_VHT_MAX_AMPDU:
            value = IEEE80211_PARAM_VHT_MAX_AMPDU;
            break;
        case ACFG_PARAM_EXT_IFACEUP_ACS:
            value = IEEE80211_PARAM_EXT_IFACEUP_ACS;
            break;
        case ACFG_PARAM_ENABLE_RTSCTS:
            value = IEEE80211_PARAM_ENABLE_RTSCTS;
            break;
        case ACFG_PARAM_VAP_ENHIND:
            value = IEEE80211_PARAM_VAP_ENHIND;
            break;
#if QCA_AIRTIME_FAIRNESS
        case ACFG_PARAM_ATF_OPT:
            value = IEEE80211_PARAM_ATF_OPT;
            break;
        case ACFG_PARAM_ATF_TXBUF_SHARE:
            value = IEEE80211_PARAM_ATF_TXBUF_SHARE;
            break;
        case ACFG_PARAM_ATF_TXBUF_MAX:
            value = IEEE80211_PARAM_ATF_TXBUF_MAX;
            break;
        case ACFG_PARAM_ATF_TXBUF_MIN:
            value = IEEE80211_PARAM_ATF_TXBUF_MIN;
            break;
        case  ACFG_PARAM_ATF_PER_UNIT:
            value = IEEE80211_PARAM_ATF_PER_UNIT;
            break;
        case  ACFG_PARAM_ATF_MAX_CLIENT:
            value = IEEE80211_PARAM_ATF_MAX_CLIENT;
            break;
#endif
        case  ACFG_PARAM_DISABLE_SELECTIVE_LEGACY_RATE:
            value = IEEE80211_PARAM_DISABLE_SELECTIVE_LEGACY_RATE_FOR_VAP;
            break;
        case  ACFG_PARAM_MODIFY_BEACON_RATE:
            value = IEEE80211_PARAM_BEACON_RATE_FOR_VAP;
            break;
        case  ACFG_PARAM_IMPLICITBF:
            value = IEEE80211_PARAM_IMPLICITBF;
            break;
        case  ACFG_PARAM_CLR_APPOPT_IE:
            value = IEEE80211_PARAM_CLR_APPOPT_IE;
            break;
        case  ACFG_PARAM_SIFS_TRIGGER_RATE:
            value = IEEE80211_PARAM_SIFS_TRIGGER_RATE;
            break;
        case ACFG_PARAM_FT_ENABLE:
            value = IEEE80211_PARAM_FT_ENABLE;
            break;
        case  ACFG_PARAM_SIFS_TRIGGER:
            value = IEEE80211_PARAM_SIFS_TRIGGER;
            break;

        default:
            value = param;
            break;
    }
    return value;
}




static int
acfg_mode2acfgmode(uint32_t param)
{
	return param;
#if not_yet
    switch(param) {
        case IW_MODE_ADHOC:
            return ACFG_OPMODE_IBSS;
            break;
        case IW_MODE_INFRA:
            return ACFG_OPMODE_STA;
            break;
        case IW_MODE_MASTER:
            return ACFG_OPMODE_HOSTAP;
            break;
        case IW_MODE_REPEAT:
            return ACFG_OPMODE_WDS;
            break;
        case IW_MODE_MONITOR:
            return ACFG_OPMODE_MONITOR;
            break;
        default:
            return -1;
        break;
    }
#endif
}

int
acfg_vap_delete(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    return ieee80211_ucfg_delete_vap(vap);
}


int
acfg_is_offload_vap(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{

    int status = -1;

    /* This function can be invoked on a VAP on
     * a local radio only. We return false by default.
    */
    return status;
}

int
acfg_add_client(void *ctx, uint16_t cmdid,
                uint8_t *buffer, int32_t Length)
{
    int status = -EINVAL;
#if WLAN_SUPPORT_SPLITMAC
    struct net_device *dev = (struct net_device *) ctx;
	osif_dev *osifp = ath_netdev_priv(dev);
#endif
    acfg_os_req_t   *req = NULL;
    acfg_add_client_t    *ptr;
    struct ieee80211_rateset lrates, htrates;
    u_int16_t vhtrates;
    u_int8_t stamac[IEEE80211_ADDR_LEN];
    uint16_t aid;
    uint8_t qos;
    int i;
    struct wlan_objmgr_vdev *vdev = NULL;
    QDF_STATUS status_vdev_ref;
    u_int16_t herates;   /* HE rate map as described in spec */

#if WLAN_SUPPORT_SPLITMAC
    vdev = osifp->ctrl_vdev;
#endif
    status_vdev_ref = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SPLITMAC_ID);
    if (QDF_IS_STATUS_ERROR(status_vdev_ref)) {
        return -1;
    }

    OS_MEMZERO(&lrates, sizeof(struct ieee80211_rateset));
    OS_MEMZERO(&htrates, sizeof(struct ieee80211_rateset));
    vhtrates = 0;
    herates = 0;

    req = (acfg_os_req_t *)buffer;

    ptr     = (acfg_add_client_t *)req->data;
    OS_MEMCPY(stamac, ptr->stamac, sizeof(stamac));
    aid = ptr->aid;
    qos = ptr->qos;

    if ((ptr->htrates.rs_nrates > IEEE80211_RATE_MAXSIZE) ||
        (ptr->lrates.rs_nrates > IEEE80211_RATE_MAXSIZE))
        return -EINVAL;

    lrates.rs_nrates = ptr->lrates.rs_nrates;
    for (i = 0; i < lrates.rs_nrates; i++)
        lrates.rs_rates[i] = ptr->lrates.rs_rates[i];

    for (i = 0; i < ptr->htrates.rs_nrates; i++) {
        htrates.rs_rates[i] = ptr->htrates.rs_rates[i];
        htrates.rs_nrates++;
    }

    /* vht, only mcs 8,9 configurable */
    vhtrates = ptr->vhtrates.rs_rates[0];

    /* 11ax HE rates */
    qdf_get_u16(ptr->herates.rs_rates, &herates);

#if WLAN_SUPPORT_SPLITMAC
    status = splitmac_api_add_client(vdev, stamac, aid, qos, (void *)&lrates, (void *)&htrates, vhtrates, herates);
#endif
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SPLITMAC_ID);
    return status;
}

int
acfg_delete_client(void *ctx, uint16_t cmdid,
                uint8_t *buffer, int32_t Length)
{
    int status = -EINVAL;
#if WLAN_SUPPORT_SPLITMAC
    struct net_device *dev = (struct net_device *) ctx;
	osif_dev *osifp = ath_netdev_priv(dev);
#endif
    acfg_os_req_t   *req = NULL;
    acfg_del_client_t    *ptr;
    struct wlan_objmgr_vdev *vdev = NULL;
    QDF_STATUS status_vdev_ref;

#if WLAN_SUPPORT_SPLITMAC
    vdev = osifp->ctrl_vdev;
#endif
    status_vdev_ref = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SPLITMAC_ID);
    if (QDF_IS_STATUS_ERROR(status_vdev_ref)) {
        return -1;
    }

    req = (acfg_os_req_t *)buffer;
    ptr     = (acfg_del_client_t *)req->data;

#if WLAN_SUPPORT_SPLITMAC
    status = splitmac_api_del_client(vdev, ptr->stamac);
#endif
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SPLITMAC_ID);
    return status;
}

int
acfg_authorize_client(void *ctx, uint16_t cmdid,
                uint8_t *buffer, int32_t Length)
{
#if WLAN_SUPPORT_SPLITMAC
    struct net_device *dev = (struct net_device *) ctx;
	osif_dev *osifp = ath_netdev_priv(dev);
#endif
    acfg_os_req_t   *req = NULL;
    acfg_authorize_client_t    *ptr;
    int status = -EINVAL;
    struct wlan_objmgr_vdev *vdev = NULL;
    QDF_STATUS status_vdev_ref;

#if WLAN_SUPPORT_SPLITMAC
    vdev = osifp->ctrl_vdev;
#endif
    status_vdev_ref = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SPLITMAC_ID);
    if (QDF_IS_STATUS_ERROR(status_vdev_ref)) {
        return -1;
    }

    req = (acfg_os_req_t *)buffer;
    ptr     = (acfg_authorize_client_t *)req->data;

#if WLAN_SUPPORT_SPLITMAC
    status = splitmac_api_authorize_client(vdev, ptr->mac, ptr->authorize);
#endif
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SPLITMAC_ID);
    return status;
}

int
acfg_del_key(void *ctx, uint16_t cmdid,
             uint8_t  *buffer, int32_t Length)
{
#if WLAN_SUPPORT_SPLITMAC
    struct net_device *dev = (struct net_device *) ctx;
	osif_dev *osifp = ath_netdev_priv(dev);
#endif
    acfg_os_req_t   *req = NULL;
    acfg_del_key_t    *ptr;
    int status = 0;
    struct wlan_objmgr_vdev *vdev = NULL;
    QDF_STATUS status_vdev_ref;

#if WLAN_SUPPORT_SPLITMAC
    vdev = osifp->ctrl_vdev;
#endif
    status_vdev_ref = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SPLITMAC_ID);
    if (QDF_IS_STATUS_ERROR(status_vdev_ref)) {
        return -1;
    }

    req = (acfg_os_req_t *)buffer;
    ptr     = (acfg_del_key_t *)req->data;

#if WLAN_SUPPORT_SPLITMAC
    status = splitmac_api_del_key(vdev, ptr->macaddr, ptr->keyix);
#endif
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SPLITMAC_ID);
	return status;
}

int
acfg_set_key(void *ctx, uint16_t cmdid,
            uint8_t  *buffer, int32_t Length)
{
#if WLAN_SUPPORT_SPLITMAC
    struct net_device *dev = (struct net_device *) ctx;
	osif_dev *osifp = ath_netdev_priv(dev);
#endif
    acfg_os_req_t   *req = NULL;
    acfg_set_key_t    *ptr;
    int status = 0;
	uint8_t cipher = IEEE80211_CIPHER_NONE;
    struct wlan_objmgr_vdev *vdev = NULL;
    QDF_STATUS status_vdev_ref;

#if WLAN_SUPPORT_SPLITMAC
    vdev = osifp->ctrl_vdev;
#endif
    status_vdev_ref = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SPLITMAC_ID);
    if (QDF_IS_STATUS_ERROR(status_vdev_ref)) {
        return -1;
    }

    req = (acfg_os_req_t *)buffer;
    ptr     = (acfg_set_key_t *)req->data;

	if (cipher == ACFG_WLAN_PROFILE_CIPHER_METH_NONE)
      return acfg_del_key(ctx, cmdid, buffer, Length);

	switch (ptr->cipher) {
	case ACFG_WLAN_PROFILE_CIPHER_METH_WEP:
        cipher = IEEE80211_CIPHER_WEP;
        break;
	case ACFG_WLAN_PROFILE_CIPHER_METH_TKIP:
        cipher = IEEE80211_CIPHER_TKIP;
        break;
	case ACFG_WLAN_PROFILE_CIPHER_METH_AES:
        cipher = IEEE80211_CIPHER_AES_CCM;
        break;
	case ACFG_WLAN_PROFILE_CIPHER_METH_AES_CMAC:
        cipher = IEEE80211_CIPHER_AES_CMAC;
        break;
	default:
		return -EINVAL;
	}
#if WLAN_SUPPORT_SPLITMAC
    status = splitmac_api_set_key(vdev, ptr->macaddr, cipher, ptr->keyix,
                                             ptr->keylen, ptr->keydata);
#endif
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SPLITMAC_ID);
	return status;
}

int
acfg_set_mu_whtlist(void *ctx, uint16_t cmdid,
            uint8_t  *buffer, int32_t Length)
{
      struct net_device *dev = (struct net_device *) ctx;
      osif_dev *osifp = ath_netdev_priv(dev);
      wlan_if_t vap = osifp->os_if;
      struct ieee80211com *ic = vap->iv_ic;

    acfg_os_req_t   *req = NULL;
    acfg_set_mu_whtlist_t    *ptr;
    int status = 1;

    req = (acfg_os_req_t *)buffer;
    ptr = (acfg_set_mu_whtlist_t *)req->data;

    status = ic->ic_ol_net80211_set_mu_whtlist(vap, ptr->macaddr, ptr->tidmask);

    return status;
}

int acfg_add_ie(void *ctx, uint16_t cmdid,
               uint8_t  *buffer, int32_t Length)
{
    int status = -EINVAL;
    struct net_device *dev = (struct net_device *) ctx;
	osif_dev *osifp = ath_netdev_priv(dev);
	wlan_if_t vap = osifp->os_if;
    acfg_os_req_t   *req = NULL;
    acfg_appie_t    *ptr;

    req = (acfg_os_req_t *)buffer;
    ptr     = (acfg_appie_t *)req->data;

    if (ptr->buflen > ACFG_MAX_APPIE_BUF)
        return -1;

#if ATH_DEBUG_SET_BEACON
    uint8_t* tmp_buf = ptr->buf;
    int32_t i;
    qdf_info("%s: %d ==dump buffer==ptr->frmtype=0x%x, ptr->buflen=0x%x ",__FUNCTION__,__LINE__,ptr->frmtype, ptr->buflen);
    for(i=0;i< ptr->buflen;i++){
        qdf_info("0x%x ",*(tmp_buf+i));
        if((i+1)%8==0) qdf_info("\n");
    }
    qdf_info("\n");
#endif

    switch (ptr->frmtype) {
    case ACFG_FRAME_BEACON:
        status = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_BEACON, ptr->buf, ptr->buflen);
        break;
    case ACFG_FRAME_PROBE_REQ:
        status = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_PROBEREQ, ptr->buf, ptr->buflen);
        break;
    case ACFG_FRAME_PROBE_RESP:
        status = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_PROBERESP, ptr->buf, ptr->buflen);
        break;
    case ACFG_FRAME_ASSOC_RESP:
        status = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_ASSOCRESP, ptr->buf, ptr->buflen);
        break;
    case ACFG_FRAME_ASSOC_REQ:
        status = wlan_mlme_set_appie(vap, IEEE80211_FRAME_TYPE_ASSOCREQ, ptr->buf, ptr->buflen);
        break;
    case ACFG_FRAME_WNM:
        status = ieee80211_wnm_set_appie(vap, ptr->buf, ptr->buflen);
        break;
    default:
        status = -1;
        break;
    }

	return status;
}

int acfg_set_vap_vendor_param(void *ctx, uint16_t cmdid,
               uint8_t  *buffer, int32_t Length)
{
    int status = 0;
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = NULL;

    req = (acfg_os_req_t *)buffer;
    status = osif_set_vap_vendor_param(dev, (acfg_vendor_param_req_t *)req->data);

    return status;
}

int acfg_get_vap_vendor_param(void *ctx, uint16_t cmdid,
               uint8_t  *buffer, int32_t Length)
{
    int status = 0;
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = NULL;

    req = (acfg_os_req_t *)buffer;
    status = osif_get_vap_vendor_param(dev, (acfg_vendor_param_req_t *)req->data);

    return status;
}

int
acfg_set_ssid(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_ssid_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
	wlan_if_t vap = osifp->os_if;
    ieee80211_ssid   tmpssid;

	req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_ssid_t *)req->data;

    OS_MEMZERO(&tmpssid, sizeof(ieee80211_ssid));

	if (ptr->len > IEEE80211_NWID_LEN)
    	ptr->len = IEEE80211_NWID_LEN;

    tmpssid.len = ptr->len;
    OS_MEMCPY(tmpssid.ssid, ptr->ssid, ptr->len);

    return ieee80211_ucfg_set_essid(vap, &tmpssid, 1);
}


int
acfg_get_ssid(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_ssid_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    ieee80211_ssid ssidlist[1];
    int des_nssid;
    int error = 0;


    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_ssid_t *)req->data;

    error = ieee80211_ucfg_get_essid(vap, ssidlist, &des_nssid);
    if(error)
        return error;

    if ((des_nssid > 0) || (ssidlist[0].len > 0))
    {
        ptr->len = ssidlist[0].len;
        OS_MEMCPY(ptr->ssid, ssidlist[0].ssid, ssidlist[0].len);
    }
    ptr->ssid[ptr->len] = '\0';
    ptr->len = ptr->len + 1;

    return 0;
}

int
acfg_set_chanswitch(void *ctx, uint16_t cmdid,
                    uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    acfg_os_req_t   *req = NULL;

    req = (acfg_os_req_t *) buffer;

    return ieee80211_ucfg_set_chanswitch(vap, req->data[0], ic->ic_chan_switch_cnt, 0);
}

int
acfg_set_chan(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    acfg_os_req_t   *req = NULL;

    req = (acfg_os_req_t *) buffer;

    if (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
          qdf_info("VAP is not ready. Saving channel [%d] to configure later",
                   req->data[0]);
          vap->iv_cfg80211_channel = (acfg_chan_t)req->data[0];
    }

    return ieee80211_ucfg_set_freq(vap, (acfg_chan_t)req->data[0]);
}

int
acfg_get_chan(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    wlan_chan_t chan;
    acfg_chan_t        *ptr;
    acfg_os_req_t   *req = NULL;

	req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_chan_t *)req->data;

	if (dev->flags & (IFF_UP | IFF_RUNNING)) {
        chan = ieee80211_ucfg_get_bss_channel(vap);
    } else {
        chan = ieee80211_ucfg_get_current_channel(vap, true);
    }
    *ptr = chan->ic_ieee;

    return 0;
}

int
acfg_get_opmode(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_opmode_t        *ptr;
    acfg_os_req_t   *req = NULL;
	struct ifmediareq imr;
    osif_dev *osnetdev = ath_netdev_priv(dev);
	uint32_t mode;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_opmode_t *)req->data;
	memset(&imr, 0, sizeof(imr));
    osnetdev->os_media.ifm_status(dev, &imr);
	if (imr.ifm_active & IFM_IEEE80211_HOSTAP)
        mode = IW_MODE_MASTER;
#if WIRELESS_EXT >= 15
    else if (imr.ifm_active & IFM_IEEE80211_MONITOR)
        mode = IW_MODE_MONITOR;
#endif
    else if (imr.ifm_active & IFM_IEEE80211_ADHOC)
        mode = IW_MODE_ADHOC;
    else if (imr.ifm_active & IFM_IEEE80211_WDS)
        mode = IW_MODE_REPEAT;
    else
        mode = IW_MODE_INFRA;

    *ptr = acfg_mode2acfgmode(mode);
    return 0;
}

int
acfg_set_freq(void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
	return 0;
}


int
acfg_get_freq(void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
	return 0;
}

int
acfg_set_reg (void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
	return 0;
}


int
acfg_get_reg (void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
	return 0;
}

int
acfg_set_rts(void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_rts_t        *ptr;
    acfg_os_req_t   *req = NULL;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_rts_t *)req->data;

    return ieee80211_ucfg_set_rts(vap, *ptr);
}


int
acfg_get_rts(void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_rts_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_rts_t *)req->data;

	*ptr = wlan_get_param(vap, IEEE80211_RTS_THRESHOLD);

    return 0;
}


int
acfg_set_frag(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_frag_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int32_t val;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_frag_t *)req->data;

	if ((*ptr < 256) || (*ptr > IEEE80211_FRAGMT_THRESHOLD_MAX)) {
        return -EINVAL;
	} else {
        val = (*ptr & ~0x1);

        return ieee80211_ucfg_set_frag(vap, val);
	}
    return 0;
}


int
acfg_get_frag(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_frag_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;


    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_frag_t *)req->data;

	*ptr = wlan_get_param(vap, IEEE80211_FRAG_THRESHOLD);

    return 0;
}


int
acfg_set_txpow(void *ctx, uint16_t cmdid,
              uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_txpow_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int retv = EOK;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_txpow_t *)req->data;

    retv = ieee80211_ucfg_set_txpow(vap, *ptr);

	return (retv != EOK) ? -EOPNOTSUPP : EOK;

}


int
acfg_get_txpow(void *ctx, uint16_t cmdid,
              uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_txpow_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int txpow, fixed;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_txpow_t *)req->data;

    ieee80211_ucfg_get_txpow(vap, &txpow, &fixed);
    *ptr = txpow;


    return 0;
}

int
acfg_set_ap(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macaddr_t    *ptr;
    acfg_os_req_t *req;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int8_t des_bssid[IEEE80211_ADDR_LEN];

    req = (acfg_os_req_t *) buffer;
    ptr = (acfg_macaddr_t *)req->data;

	IEEE80211_ADDR_COPY(des_bssid, ptr->addr);

    return ieee80211_ucfg_set_ap(vap, &des_bssid);
}

int
acfg_get_ap(void *ctx, uint16_t cmdid,
           uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macaddr_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osnetdev = ath_netdev_priv(dev);
    wlan_if_t vap = osnetdev->os_if;
    u_int8_t bssid[IEEE80211_ADDR_LEN];
    int status;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_macaddr_t *)req->data;

    status = ieee80211_ucfg_get_ap(vap, bssid);

    IEEE80211_ADDR_COPY(ptr->addr, bssid);

    return status;
}

int
acfg_set_vap_param(void *ctx, uint16_t cmdid,
                  uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_param_req_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
	int param, value;

    req = (acfg_os_req_t *) buffer;
    ptr  = (acfg_param_req_t *)req->data;

	param = acfg_acfg2ieee(ptr->param);
	value = ptr->val;

    return ieee80211_ucfg_setparam(vap, param, value, (char *)ptr);
}

int
acfg_get_vap_param(void *ctx, uint16_t cmdid,
                  uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_param_req_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
	int retv = 0, param, value = 0;

    req = (acfg_os_req_t *) buffer;
    ptr  = (acfg_param_req_t *)req->data;

    param = acfg_acfg2ieee(ptr->param);
    ptr->val = 0;

    retv = ieee80211_ucfg_getparam(vap, param, &value);

    ptr->val = value;
    ptr->param = 0;

    return retv;
}

int
acfg_get_rate(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    u_int32_t        *ptr;
    acfg_os_req_t   *req = NULL;
    wlan_if_t vap = NETDEV_TO_VAP(dev);

    req = (acfg_os_req_t *) buffer;
    ptr     = (u_int32_t *)req->data;

    *ptr = ieee80211_ucfg_get_maxphyrate(vap);

    return 0;
}

int
acfg_set_phymode(void *ctx, uint16_t cmdid,
                uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t *req;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
	char s[30];      /* big enough */
    int len = 0;
    req = (acfg_os_req_t *) buffer;
	len = strlen(phymode_strings[req->data[0]]) + 1;
	if (len > sizeof(s)) {
		len = sizeof(s);
	}
	memcpy(s, (char *)phymode_strings[req->data[0]], len);
	s[sizeof(s)-1] = '\0';

    return ieee80211_ucfg_set_phymode(vap, s, len);
}

int
acfg_set_enc(void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_encode_t   *ptr;
    acfg_os_req_t   *req = NULL;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    void *key_buff;
    int status;

    req = (acfg_os_req_t *) buffer;
    ptr = (acfg_encode_t *)req->data;

    if(ptr->len > ACFG_ENCODING_TOKEN_MAX) {
        return -EINVAL;
    }

    key_buff = (void*)qdf_mem_malloc(ptr->len);
    if(!key_buff) {
        return -ENOMEM;
    }

    if (__xcopy_from_user(key_buff, ptr->buff, ptr->len)) {
        qdf_mem_free(key_buff);
        return -EFAULT;
    }

    status = ieee80211_ucfg_set_encode(vap, ptr->len, ptr->flags, key_buff);

    qdf_mem_free(key_buff);
    return status;
}

int
acfg_set_rate(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_rate_t        *ptr;
    acfg_os_req_t   *req = NULL;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int value;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_rate_t *)req->data;

	if (*ptr) {
        unsigned int rate = *ptr;
        if (rate >= 1000) {
            /* convert rate to index */
            int i;
            int array_size = sizeof(legacy_rate_idx)/sizeof(legacy_rate_idx[0]);
            rate /= 1000;
            for (i = 0; i < array_size; i++) {
                if (rate == legacy_rate_idx[i][0]) {
                    rate = legacy_rate_idx[i][1];
                    break;
                }
            }
            if (i == array_size) return -EINVAL;
        }
        value = rate;
    } else {
		value = IEEE80211_FIXED_RATE_NONE;
	}

    return ieee80211_ucfg_set_rate(vap, value);
}

int
acfg_get_rssi(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_rssi_t    *ptr;
    acfg_vendor_t    *vendor;
    acfg_os_req_t   *req_rssi = NULL;
    uint8_t *results = NULL;
	int fromvirtual = 0;
	struct iwreq req;

    req_rssi = (acfg_os_req_t *) buffer;
    ptr     = (acfg_rssi_t *)req_rssi->data;


    memset(&req, 0, sizeof(struct iwreq));
    results = qdf_mem_malloc(sizeof(acfg_vendor_t)+sizeof(acfg_rssi_t));
    if (!results) {
        qdf_print("%s,line:%d alloc failed",__func__,__LINE__);
        return -ENOMEM;
    }

    vendor = (acfg_vendor_t *)results;
    vendor->cmd = IOCTL_GET_MASK | IEEE80211_IOCTL_RSSI;

    req.u.data.pointer = (uint8_t *)results;
    req.u.data.length  = sizeof(acfg_vendor_t)+sizeof(acfg_rssi_t);
    req.u.data.flags   = 1;
	if (strncmp(dev->name, "wifi", 4) == 0) {
		fromvirtual = 0;
	} else {
		fromvirtual = 1;
	}
#ifdef ATH_SUPPORT_LINUX_VENDOR
	osif_ioctl_vendor(dev, (struct ifreq *)&req, fromvirtual);
#endif
    memcpy((char *)ptr, ( (char *)results + sizeof(acfg_vendor_t)),
            req.u.data.length - sizeof (acfg_vendor_t));

    kfree (results);

    return 0;
}

int
acfg_get_phymode(void *ctx, uint16_t cmdid,
                uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t *req;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    u_int16_t i, length = 0;
    char modestr[30] = {0};

    req = (acfg_os_req_t *) buffer;

    ieee80211_ucfg_get_phymode(vap, modestr, &length, PHY_MODE);

    for (i = 0; i < ACFG_PHYMODE_INVALID; i++)
    {
        if (strncmp (phymode_strings[i],
                    modestr, length) == 0)
        {
            req->data[0] = i;
            break;
        }
    }

    return 0;
}

int
acfg_mon_enable_filter(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t *req;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int32_t filter_type = 0;

    if(IEEE80211_M_MONITOR != vap->iv_opmode) {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"Not monitor VAP!\n");
       return -EINVAL;
    }

    req = (acfg_os_req_t *) buffer;
    filter_type = req->data[0];

    return (wlan_set_param(vap, IEEE80211_RX_FILTER_MONITOR, filter_type));
}

int
acfg_mon_listmac(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
#if WLAN_SUPPORT_SPLITMAC
   struct net_device *dev = (struct net_device *) ctx;
   osif_dev *osifp = ath_netdev_priv(dev);
   wlan_if_t vap = osifp->os_if;
#endif
   int rc = 0;
#if WLAN_SUPPORT_SPLITMAC
   rc = wlan_mon_listmac(vap);
#endif
   return rc;
}


int
acfg_mon_addmac(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
   acfg_macaddr_t    *ptr;
   acfg_os_req_t *req;
   struct net_device *dev = (struct net_device *) ctx;
   osif_dev *osifp = ath_netdev_priv(dev);
   wlan_if_t vap = osifp->os_if;
   int rc = 0;
   enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
   if(opmode == IEEE80211_M_MONITOR) {
       req = (acfg_os_req_t *) buffer;
       ptr     = (acfg_macaddr_t *)req->data;
       qdf_info("%s", ether_sprintf(ptr->addr));
       rc = wlan_mon_addmac(vap, ptr->addr);
       return rc;
   }
   else
       return -EINVAL;

   return rc;
}

int
acfg_mon_delmac(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
   acfg_macaddr_t    *ptr;
   acfg_os_req_t *req;
   struct net_device *dev = (struct net_device *) ctx;
   osif_dev *osifp = ath_netdev_priv(dev);
   wlan_if_t vap = osifp->os_if;
   int rc = 0;
   enum ieee80211_opmode opmode = wlan_vap_get_opmode(vap);
   if(opmode == IEEE80211_M_MONITOR) {
       req = (acfg_os_req_t *) buffer;
       ptr     = (acfg_macaddr_t *)req->data;
       qdf_info("in the driver delmac");
       qdf_info("%s", ether_sprintf(ptr->addr));
       rc = wlan_mon_delmac(vap, ptr->addr);
       return rc;
   } else {
       return -EINVAL;
   }
}

int
acfg_acl_addmac(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macaddr_t    *ptr;
    acfg_os_req_t *req;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
	int rc;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_macaddr_t *)req->data;

    qdf_info("%s", ether_sprintf(ptr->addr));
    rc = wlan_set_acl_add(vap, ptr->addr, IEEE80211_ACL_FLAG_ACL_LIST_1);
    return rc;
}


int
acfg_acl_getmac(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macacl_t    *ptr;
    acfg_os_req_t *req;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
	int rc=0;
    u_int8_t *macList;
    int i, num_mac;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_macacl_t *)req->data;

    memset(ptr->macaddr, 0, sizeof(*ptr) - sizeof(ptr->num));
	macList = (u_int8_t *)OS_MALLOC(osifp->os_handle,
								(IEEE80211_ADDR_LEN * 256), GFP_KERNEL);
	if (!macList) {
    	return -EFAULT;
    }
    rc = wlan_get_acl_list(vap, macList, (IEEE80211_ADDR_LEN * 256), &num_mac, IEEE80211_ACL_FLAG_ACL_LIST_1);
	if(rc) {
    	if (macList) {
        	OS_FREE(macList);
        }
        return -EFAULT;
    }
	for (i = 0; i < num_mac; i++) {
    	memcpy(ptr->macaddr[i], &macList[i * IEEE80211_ADDR_LEN],
												IEEE80211_ADDR_LEN);
    }
    ptr->num = num_mac;
	if (macList) {
    	OS_FREE(macList);
    }
    return rc;
}


int
acfg_acl_delmac(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{

    struct net_device *dev = (struct net_device *) ctx;
    acfg_macaddr_t    *ptr;
    acfg_os_req_t *req;
	osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
	int rc;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_macaddr_t *)req->data;

    rc = wlan_set_acl_remove(vap, ptr->addr, IEEE80211_ACL_FLAG_ACL_LIST_1);
    return rc;
}

/* Addmac, Delmac and Getmac functions for secondary ACL
 * implementation.
 */
int
acfg_acl_addmac_secondary(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macaddr_t *ptr;
    acfg_os_req_t *req;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int rc;

    if(length < sizeof(acfg_req_cmd_t) + sizeof(acfg_macaddr_t)) {
        return -EINVAL;
    }
    req = (acfg_os_req_t *) buffer;
    ptr = (acfg_macaddr_t *)req->data;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
                            "%s\n", ether_sprintf(ptr->addr));
    rc = wlan_set_acl_add(vap, ptr->addr, IEEE80211_ACL_FLAG_ACL_LIST_2);
    return rc;
}

int
acfg_acl_getmac_secondary(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macacl_t *ptr;
    acfg_os_req_t *req;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int rc = 0;
    u_int8_t *mac_list;
    int i, num_mac;

    if(length < sizeof(acfg_req_cmd_t) + sizeof(acfg_macacl_t)) {
        return -EINVAL;
    }

    req = (acfg_os_req_t *) buffer;
    ptr = (acfg_macacl_t *)req->data;

    memset(ptr->macaddr, 0, sizeof(*ptr) - sizeof(ptr->num));
    mac_list = (u_int8_t *)OS_MALLOC(osifp->os_handle,
                                    (IEEE80211_ADDR_LEN * 256), GFP_KERNEL);
    if (!mac_list) {
        return -EFAULT;
    }
    rc = wlan_get_acl_list(vap, mac_list, (IEEE80211_ADDR_LEN * 256),
                                    &num_mac, IEEE80211_ACL_FLAG_ACL_LIST_2);
    if(rc) {
        OS_FREE(mac_list);
        return -EFAULT;
    }
    for (i = 0; i < num_mac; i++) {
        memcpy(ptr->macaddr[i], &mac_list[i * IEEE80211_ADDR_LEN], IEEE80211_ADDR_LEN);
    }
    ptr->num = num_mac;
    OS_FREE(mac_list);
    return rc;
}

int
acfg_acl_delmac_secondary(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_macaddr_t *ptr;
    acfg_os_req_t *req;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    int rc;

    if(length < sizeof(acfg_req_cmd_t) + sizeof(acfg_macaddr_t)) {
        return -EINVAL;
    }

    req = (acfg_os_req_t *) buffer;
    ptr = (acfg_macaddr_t *)req->data;

    rc = wlan_set_acl_remove(vap, ptr->addr, IEEE80211_ACL_FLAG_ACL_LIST_2);
    return rc;
}

int
acfg_vap_atf_addssid(void *ctx, uint16_t cmdid,
        uint8_t *buffer, int32_t Length)
{
    int rc = 0;
#if QCA_AIRTIME_FAIRNESS
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    acfg_os_req_t *req;
    struct ssid_val buf;

    req = (acfg_os_req_t *) buffer;

    memcpy(&buf, req->data, sizeof(struct ssid_val));

    rc = ucfg_atf_set_ssid(vap->vdev_obj, &buf);
#endif
    return rc;
}

int
acfg_vap_atf_delssid(void *ctx, uint16_t cmdid,
        uint8_t *buffer, int32_t Length)
{
    int rc = 0;
#if QCA_AIRTIME_FAIRNESS
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    acfg_os_req_t *req;
    struct ssid_val buf;

    req = (acfg_os_req_t *) buffer;

    memcpy(&buf, req->data, sizeof(struct ssid_val));

    rc = ucfg_atf_delete_ssid(vap->vdev_obj, &buf);
#endif
    return rc;
}

int
acfg_vap_atf_addsta(void *ctx, uint16_t cmdid,
        uint8_t *buffer, int32_t Length)
{
    int rc = 0;
#if QCA_AIRTIME_FAIRNESS
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    acfg_os_req_t *req;
    struct sta_val buf;

    req = (acfg_os_req_t *) buffer;

    memcpy(&buf, req->data, sizeof(struct sta_val));

    rc = ucfg_atf_set_sta(vap->vdev_obj, &buf);
#endif
    return rc;
}

int
acfg_vap_atf_delsta(void *ctx, uint16_t cmdid,
        uint8_t *buffer, int32_t Length)
{
    int rc = 0;
#if QCA_AIRTIME_FAIRNESS
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    acfg_os_req_t *req;
    struct sta_val buf;

    req = (acfg_os_req_t *) buffer;

    memcpy(&buf, req->data, sizeof(struct sta_val));

    rc = ucfg_atf_delete_sta(vap->vdev_obj, &buf);
#endif
    return rc;
}

static int
acfg_set_ratemask(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_ratemask_t   *ptr = (acfg_ratemask_t *)req->data;

    return ieee80211_ucfg_rcparams_setratemask(vap, ptr->preamble,
                                                  ptr->mask_lower32,
                                                  ptr->mask_higher32,
                                                  ptr->mask_lower32_2);
}

int
acfg_assoc_sta_info(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_sta_info_req_t *ptr = (acfg_sta_info_req_t *)req->data;
    struct ieee80211req_sta_info *si;
    int status = 0;
    uint32_t data_length = 0;

    data_length = ptr->len;
    data_length = ieee80211_ucfg_getstaspace(vap);
    if (data_length == 0) {
        ptr->len = 0;
        return 0;
    }

    /* Alloc temp buffer for all associated STAs */
    si = OS_MALLOC(osifp->os_handle, data_length, GFP_KERNEL);
    if (si == NULL)
        return -ENOMEM;
    OS_MEMZERO(si, data_length);

    status = ieee80211_ucfg_getstainfo(vap, si, &data_length);

    if (ptr->len > data_length)
        ptr->len = data_length;

    /* Copy ptr->len of acfg_sta_info_t elements */
    if(copy_to_user(ptr->info, si, ptr->len)) {
            return -EFAULT;
    }

    OS_FREE(si);

    return status;
}

/**
 * @brief get supported rates in beacon IE
 */
static int
acfg_get_beacon_supported_rates(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_rateset_t  *target_rs = (acfg_rateset_t *)req->data;
    enum ieee80211_phymode mode = wlan_get_desired_phymode(vap);
    struct ieee80211_rateset *rs = &(vap->iv_op_rates[mode]);
    uint8_t j;

    if(target_rs->rs_nrates < rs->rs_nrates){
        return -EINVAL;
    }

    for (j=0; j<(rs->rs_nrates); j++) {
        target_rs->rs_rates[j] = rs->rs_rates[j];
    }
    target_rs->rs_nrates = rs->rs_nrates;

    return 0;
}

static int
acfg_set_muedca_ecwmin(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    return ieee80211_ucfg_set_muedcaparams((void *)osifp,
                IEEE80211_MUEDCAPARAMS_ECWMIN, edca_param->ac, edca_param->val);
}

static int
acfg_get_muedca_ecwmin(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    edca_param->val = ieee80211_ucfg_get_muedcaparams((void *)osifp,
                            IEEE80211_MUEDCAPARAMS_ECWMIN, edca_param->ac);
    return 0;
}

static int
acfg_set_muedca_ecwmax(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    return ieee80211_ucfg_set_muedcaparams((void *)osifp,
                IEEE80211_MUEDCAPARAMS_ECWMAX, edca_param->ac, edca_param->val);
}

static int
acfg_get_muedca_ecwmax(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    edca_param->val = ieee80211_ucfg_get_muedcaparams((void *)osifp,
                            IEEE80211_MUEDCAPARAMS_ECWMAX, edca_param->ac);
    return 0;
}

static int
acfg_set_muedca_aifsn(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    return ieee80211_ucfg_set_muedcaparams((void *)osifp,
                IEEE80211_MUEDCAPARAMS_AIFSN, edca_param->ac, edca_param->val);
}

static int
acfg_get_muedca_aifsn(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    edca_param->val = ieee80211_ucfg_get_muedcaparams((void *)osifp,
                            IEEE80211_MUEDCAPARAMS_AIFSN, edca_param->ac);
    return 0;
}

static int
acfg_set_muedca_acm(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;

    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    return ieee80211_ucfg_set_muedcaparams((void *)osifp,
                IEEE80211_MUEDCAPARAMS_ACM, edca_param->ac, edca_param->val);
}

static int
acfg_get_muedca_acm(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    edca_param->val = ieee80211_ucfg_get_muedcaparams((void *)osifp,
                            IEEE80211_MUEDCAPARAMS_ACM, edca_param->ac);
    return 0;
}

static int
acfg_set_muedca_timer(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;

    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    return ieee80211_ucfg_set_muedcaparams((void *)osifp,
                IEEE80211_MUEDCAPARAMS_TIMER, edca_param->ac, edca_param->val);
}

static int
acfg_get_muedca_timer(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_muedca_param_t *edca_param = (acfg_muedca_param_t *)req->data;

    edca_param->val = ieee80211_ucfg_get_muedcaparams((void *)osifp,
                            IEEE80211_MUEDCAPARAMS_TIMER, edca_param->ac);
    return 0;
}

#if QCA_SUPPORT_GPR
static int
acfg_send_gpr_cmd(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    osif_dev  *osifp = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    wlan_if_t vap = osifp->os_if;
    acfg_gpr_cmd_param_t *gpr_param = (acfg_gpr_cmd_param_t *)(req->data);

    return ieee80211_ucfg_send_gprparams(vap, gpr_param->command);
}
#endif

acfg_dispatch_entry_t acfgdispatchentry[] =
{
    { 0,                      ACFG_REQ_FIRST_VAP         , 0 }, //--> 12
    { acfg_set_ssid,          ACFG_REQ_SET_SSID          , 0 },
    { acfg_get_ssid,          ACFG_REQ_GET_SSID          , 0 },
    { acfg_set_chanswitch,    ACFG_REQ_DOTH_CHSWITCH     , 0 },
    { acfg_set_chan,          ACFG_REQ_SET_CHANNEL       , 0 },
    { acfg_get_chan,          ACFG_REQ_GET_CHANNEL       , 0 },
    { acfg_vap_delete,        ACFG_REQ_DELETE_VAP        , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_get_opmode,        ACFG_REQ_GET_OPMODE        , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_set_freq,          ACFG_REQ_SET_FREQUENCY     , 0 },
    { acfg_get_freq,          ACFG_REQ_GET_FREQUENCY     , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_set_rts,           ACFG_REQ_SET_RTS           , 0 },
    { acfg_get_rts,           ACFG_REQ_GET_RTS           , 0 },
    { acfg_set_frag,          ACFG_REQ_SET_FRAG          , 0 },
    { acfg_get_frag,          ACFG_REQ_GET_FRAG          , 0 },
    { acfg_set_txpow,         ACFG_REQ_SET_TXPOW         , 0 },
    { acfg_get_txpow,         ACFG_REQ_GET_TXPOW         , 0 },
    { acfg_set_ap,            ACFG_REQ_SET_AP            , 0 },
    { acfg_get_ap,            ACFG_REQ_GET_AP            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_set_vap_param,     ACFG_REQ_SET_VAP_PARAM     , 0 },
    { acfg_get_vap_param,     ACFG_REQ_GET_VAP_PARAM     , 0 },
    { acfg_get_rate,          ACFG_REQ_GET_RATE          , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_set_phymode,       ACFG_REQ_SET_PHYMODE       , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_set_enc,           ACFG_REQ_SET_ENCODE        , 0 },
    { acfg_set_rate,          ACFG_REQ_SET_RATE          , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_get_rssi,          ACFG_REQ_GET_RSSI          , 0 },
    { acfg_set_reg,           ACFG_REQ_SET_REG           , 0 },
    { acfg_get_reg,           ACFG_REQ_GET_REG           , 0 },
    { 0,                      ACFG_REQ_SET_TESTMODE      , 0 },
    { 0,                      ACFG_REQ_GET_TESTMODE      , 0 },
    { 0,                      ACFG_REQ_GET_CUSTDATA      , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_acl_addmac_secondary, ACFG_REQ_ACL_ADDMAC_SEC , 0 },
    { acfg_acl_getmac_secondary, ACFG_REQ_ACL_GETMAC_SEC , 0 },
    { acfg_acl_delmac_secondary, ACFG_REQ_ACL_DELMAC_SEC , 0 },
    { acfg_get_phymode,       ACFG_REQ_GET_PHYMODE       , 0 },
    { acfg_acl_addmac,        ACFG_REQ_ACL_ADDMAC        , 0 },
    { acfg_acl_getmac,        ACFG_REQ_ACL_GETMAC        , 0 },
    { acfg_acl_delmac,        ACFG_REQ_ACL_DELMAC        , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_is_offload_vap,    ACFG_REQ_IS_OFFLOAD_VAP    , 0 },
    { acfg_add_client,        ACFG_REQ_ADD_CLIENT        , 0 },
    { acfg_delete_client,     ACFG_REQ_DEL_CLIENT        , 0 },
    { acfg_authorize_client,  ACFG_REQ_AUTHORIZE_CLIENT  , 0 },
    { acfg_set_key,           ACFG_REQ_SET_KEY           , 0 },
    { acfg_del_key,           ACFG_REQ_DEL_KEY           , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },  //--> security requests
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_add_ie,            ACFG_REQ_SET_APPIEBUF      , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_assoc_sta_info,    ACFG_REQ_GET_ASSOC_STA_INFO, 0 },
    { acfg_acl_getmac,        ACFG_REQ_GET_MAC_ADDR      , 0 },
    { acfg_set_vap_vendor_param, ACFG_REQ_SET_VAP_VENDOR_PARAM, 0 },
    { acfg_get_vap_vendor_param, ACFG_REQ_GET_VAP_VENDOR_PARAM, 0 },
    { acfg_set_ratemask,      ACFG_REQ_SET_RATEMASK      , 0 },
    { acfg_get_beacon_supported_rates, ACFG_REQ_GET_BEACON_SUPPORTED_RATES   , 0 },
    { acfg_set_mu_whtlist,        ACFG_REQ_SET_MU_WHTLIST    , 0 },
    { acfg_mon_addmac,        ACFG_REQ_MON_ADDMAC        , 0 },
    { acfg_mon_delmac,        ACFG_REQ_MON_DELMAC        , 0 },
    { acfg_mon_listmac,       ACFG_REQ_MON_LISTMAC       , 0 },
    { acfg_mon_enable_filter,  ACFG_REQ_MON_ENABLE_FILTER , 0 },
    { acfg_vap_atf_addssid,   ACFG_REQ_SET_ATF_ADDSSID   , 0 },
    { acfg_vap_atf_delssid,   ACFG_REQ_SET_ATF_DELSSID   , 0 },
    { acfg_vap_atf_addsta,    ACFG_REQ_SET_ATF_ADDSTA    , 0 },
    { acfg_vap_atf_delsta,    ACFG_REQ_SET_ATF_DELSTA    , 0 },
    { acfg_set_muedca_ecwmin, ACFG_REQ_SET_MUEDCA_ECWMIN , 0 },
    { acfg_get_muedca_ecwmin, ACFG_REQ_GET_MUEDCA_ECWMIN , 0 },
    { acfg_set_muedca_ecwmax, ACFG_REQ_SET_MUEDCA_ECWMAX , 0 },
    { acfg_get_muedca_ecwmax, ACFG_REQ_GET_MUEDCA_ECWMAX , 0 },
    { acfg_set_muedca_aifsn,  ACFG_REQ_SET_MUEDCA_AIFSN , 0 },
    { acfg_get_muedca_aifsn,  ACFG_REQ_GET_MUEDCA_AIFSN , 0 },
    { acfg_set_muedca_acm,    ACFG_REQ_SET_MUEDCA_ACM , 0 },
    { acfg_get_muedca_acm,    ACFG_REQ_GET_MUEDCA_ACM , 0 },
    { acfg_set_muedca_timer,  ACFG_REQ_SET_MUEDCA_TIMER , 0 },
    { acfg_get_muedca_timer,  ACFG_REQ_GET_MUEDCA_TIMER , 0 },
#if QCA_SUPPORT_GPR
    { acfg_send_gpr_cmd,      ACFG_REQ_SEND_GPR_CMD     , 0 },
#endif
    { 0,                      ACFG_REQ_LAST_VAP          , 0 },  //--> 98
};


int
acfg_handle_vap_ioctl(struct net_device *dev, void *data)
{
    acfg_os_req_t   *req = NULL;
    uint32_t    req_len = sizeof(struct acfg_os_req);
    int32_t status = 0;
    uint32_t i;
    bool cmd_found = false;

    req = (acfg_os_req_t *)data;

    qdf_info("\n the cmd id is %d",req->cmd);
    /* Iterate the dispatch table to find the handler
     * for the command received from the user */
    for (i = 0; i < sizeof(acfgdispatchentry)/sizeof(acfgdispatchentry[0]); i++)
    {
        if (acfgdispatchentry[i].cmdid == req->cmd) {
            cmd_found = true;
            break;
       }
    }

    if(cmd_found == false){
        status = -1;
        qdf_print("ACFG ioctl CMD %d failed ",req->cmd);
        goto done;
    }

    if (acfgdispatchentry[i].cmd_handler == NULL) {
        status = -1;
        qdf_print("ACFG ioctl CMD %d failed", acfgdispatchentry[i].cmdid);
        goto done;
    }

    status = acfgdispatchentry[i].cmd_handler(dev,
                    req->cmd, (uint8_t *)req, req_len);

done:
    return status;
}

int
acfg_handle_vap_config(struct net_device *dev, void *data)
{
    return acfg_handle_vap_ioctl(dev, data);
}

void acfg_send_event(struct net_device *dev, osdev_t osdev,
             acfg_event_type_t type, acfg_event_data_t *acfg_event)
{
	acfg_os_event_t     *event;
    acfg_assoc_t    *assoc;
    acfg_dauth_t    *deauth;
    acfg_auth_t     *auth;
    acfg_leave_ap_t   *node_leave_ap;
    acfg_disassoc_t    *disassoc;
    acfg_pbc_ev_t     *pbc;
    acfg_chan_start_t *chan;
    acfg_radar_t      *radar;
    int i;
    acfg_session_t    *session;
    acfg_tx_overflow_t  *overflow;
    acfg_gpio_t       *gpio;
    acfg_mgmt_rx_info_t *ri;
    acfg_wdt_event_t  *wdt;
    acfg_nf_dbr_dbm_t   *nf_dbr_dbm;
    acfg_packet_power_t   *packet_power;
    acfg_cac_start_t  *cac_start;
    acfg_up_after_cac_t  *up_after_cac;
    acfg_kickout_t      *kickout;
    acfg_assoc_failure_t *assoc_fail;
    acfg_chan_stats_t *chan_stats;
    acfg_exceed_max_client_t *exceed_max_client;
#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
    acfg_smart_log_fw_pktlog_stop_t *slfwpktlog_stop;
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */

    event = (acfg_os_event_t *)qdf_mem_malloc_atomic(sizeof(acfg_os_event_t));
    if (event == NULL)
        return;

    switch (type) {
	case WL_EVENT_TYPE_DISASSOC_AP:
        case WL_EVENT_TYPE_DISASSOC_STA:
            if (type == WL_EVENT_TYPE_DISASSOC_AP) {
                event->id = ACFG_EV_DISASSOC_AP;
            } else if (type == WL_EVENT_TYPE_DISASSOC_STA) {
                event->id = ACFG_EV_DISASSOC_STA;
            }
            disassoc = (void *)&event->data;
			disassoc->frame_send = 0;
            disassoc->reason = acfg_event->reason;
            if (acfg_event->downlink == 0) {
                disassoc->status = acfg_event->result;
                disassoc->frame_send = 1;
            }
            memcpy(disassoc->macaddr,  acfg_event->addr,
                                            ACFG_MACADDR_LEN);
            break;

		case WL_EVENT_TYPE_DEAUTH_AP:
        case WL_EVENT_TYPE_DEAUTH_STA:
            if (type == WL_EVENT_TYPE_DEAUTH_AP) {
                event->id = ACFG_EV_DEAUTH_AP;
            } else if (type == WL_EVENT_TYPE_DEAUTH_STA) {
                event->id = ACFG_EV_DEAUTH_STA;
            }
            deauth = (void *)&event->data;
			deauth->frame_send = 0;
            if (acfg_event->downlink == 0) {
                deauth->frame_send = 1;
                deauth->status = acfg_event->result;
            } else {
            	deauth->reason = acfg_event->reason;
			}
            memcpy(deauth->macaddr,  acfg_event->addr,
                                            ACFG_MACADDR_LEN);
            break;

		case WL_EVENT_TYPE_ASSOC_AP:
        case WL_EVENT_TYPE_ASSOC_STA:
            if (type == WL_EVENT_TYPE_ASSOC_AP) {
                event->id = ACFG_EV_ASSOC_AP;
            } else if (type == WL_EVENT_TYPE_ASSOC_STA) {
                event->id = ACFG_EV_ASSOC_STA;
            }
            assoc = (void *)&event->data;
			assoc->frame_send = 0;
            assoc->status = acfg_event->result;
            if (acfg_event->downlink == 0) {
                assoc->frame_send = 1;
            } else {
            	memcpy(assoc->bssid,  acfg_event->addr,
                                            ACFG_MACADDR_LEN);
			}
            break;

		case WL_EVENT_TYPE_AUTH_AP:
        case WL_EVENT_TYPE_AUTH_STA:
            if (type == WL_EVENT_TYPE_AUTH_AP) {
                event->id = ACFG_EV_AUTH_AP;
            } else if (type == WL_EVENT_TYPE_AUTH_STA) {
                event->id = ACFG_EV_AUTH_STA;
            }
            auth = (void *)&event->data;
			auth->frame_send = 0;
            auth->status = acfg_event->result;
            if (acfg_event->downlink == 0) {
                auth->frame_send = 1;
            } else {
            	memcpy(auth->macaddr, acfg_event->addr, ACFG_MACADDR_LEN);
			}

            break;

		case WL_EVENT_TYPE_NODE_LEAVE:
			event->id = ACFG_EV_NODE_LEAVE;
			node_leave_ap = (void *)&event->data;
			node_leave_ap->reason = acfg_event->reason;
			memcpy(node_leave_ap->mac,  acfg_event->addr,
											ACFG_MACADDR_LEN);
			break;
	    case WL_EVENT_TYPE_PUSH_BUTTON:

			if (jiffies_to_msecs(jiffies - prev_jiffies)  < 2000 ) {
				prev_jiffies = jiffies;
				break;
			}
			prev_jiffies = jiffies;
            pbc = (void *)&event->data;
            event->id = ACFG_EV_PUSH_BUTTON;
            strlcpy(pbc->ifname, dev->name, IFNAMSIZ);
            break;
        case WL_EVENT_TYPE_CHAN:
            chan = (void *)&event->data;
            event->id = ACFG_EV_CHAN_START;
            chan->freq = acfg_event->freqs[0];
            chan->reason = acfg_event->reason;
            break;
        case WL_EVENT_TYPE_STA_SESSION:
            session = (void *)&event->data;
            event->id = ACFG_EV_STA_SESSION;
            session->reason = acfg_event->reason;
            memcpy(session->mac, acfg_event->addr, ACFG_MACADDR_LEN);
            break;
        case WL_EVENT_TYPE_RADAR:
            radar = (void *)&event->data;
            event->id = ACFG_EV_RADAR;
            radar->count = acfg_event->count;
            for (i = 0; i < radar->count; i++)
              radar->freqs[i] = acfg_event->freqs[i];
            break;
        case WL_EVENT_TYPE_TX_OVERFLOW:
            overflow = (void *)&event->data;
            event->id = ACFG_EV_TX_OVERFLOW;
    	    break;
        case WL_EVENT_TYPE_GPIO_INPUT:
            gpio = (void *)&event->data;
            qdf_info("GPIO input event");
            event->id = ACFG_EV_GPIO_INPUT;
            gpio->num = acfg_event->gpio_num;
            break;
        case WL_EVENT_TYPE_MGMT_RX_INFO:
            ri = (void *)&event->data;
            event->id = ACFG_EV_MGMT_RX_INFO;
            memcpy(ri, &(acfg_event->mgmt_ri), sizeof(acfg_mgmt_rx_info_t));
            break;
        case WL_EVENT_TYPE_WDT_EVENT:
            event->id = ACFG_EV_WDT_EVENT;
            wdt = (void *)&event->data;
            wdt->reason = acfg_event->reason;
    	    break;
        case WL_EVENT_TYPE_NF_DBR_DBM_INFO:
            event->id = ACFG_EV_NF_DBR_DBM_INFO;
            nf_dbr_dbm =(void *)&event->data;
            memcpy(nf_dbr_dbm, &(acfg_event->nf_dbr_dbm), sizeof(acfg_nf_dbr_dbm_t));
            break;
        case WL_EVENT_TYPE_PACKET_POWER_INFO:
            event->id = ACFG_EV_PACKET_POWER_INFO;
            packet_power =(void *)&event->data;
            memcpy(packet_power, &(acfg_event->packet_power), sizeof(acfg_packet_power_t));
            break;
        case WL_EVENT_TYPE_CAC_START:
            cac_start = (void *)&event->data;
            event->id = ACFG_EV_CAC_START;
            break;
        case WL_EVENT_TYPE_UP_AFTER_CAC:
            up_after_cac = (void *)&event->data;
            event->id = ACFG_EV_UP_AFTER_CAC;
            break;
        case WL_EVENT_TYPE_QUICK_KICKOUT:
            kickout = (void *)&event->data;
            event->id = ACFG_EV_QUICK_KICKOUT;
            memcpy(kickout->macaddr, acfg_event->kick_node_mac, ACFG_MACADDR_LEN);
            break;
        case WL_EVENT_TYPE_ASSOC_FAILURE:
            event->id = ACFG_EV_ASSOC_FAILURE;
            assoc_fail = (void *)&event->data;
            assoc_fail->reason = acfg_event->reason;
            memcpy(assoc_fail->macaddr,  acfg_event->addr, ACFG_MACADDR_LEN);
            break;
        case WL_EVENT_TYPE_CHAN_STATS:
            event->id = ACFG_EV_CHAN_STATS;
            chan_stats = (void *)&event->data;
            memcpy(chan_stats, &(acfg_event->chan_stats),
                   sizeof(acfg_chan_stats_t));
            break;
        case WL_EVENT_TYPE_EXCEED_MAX_CLIENT:
            exceed_max_client = (void *)&event->data;
            event->id = ACFG_EV_EXCEED_MAX_CLIENT;
            break;
#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
        case WL_EVENT_TYPE_SMART_LOG_FW_PKTLOG_STOP:
            event->id = ACFG_EV_SMART_LOG_FW_PKTLOG_STOP;
            slfwpktlog_stop = (void *)&event->data;
            slfwpktlog_stop->reason = acfg_event->reason;
            break;
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */
		default:
			qdf_print("Unknown event");
			return;
	}

    acfg_net_indicate_event(dev, osdev, event, ACFG_NETLINK_SENT_EVENT, 0, 0);
    qdf_mem_free(event);
}

#if ACFG_NETLINK_TX

static
void acfg_send_msg_response(struct ieee80211com *ic, struct acfg_offchan_resp *acfg_hdr,
                            int process_pid, acfg_offchan_stat_t *stat, struct ieee80211vap *vap)
{
    struct acfg_offchan_resp *acfg_hdr_rsp;
    acfg_netlink_pvt_t *acfg_nl;
    osdev_t osdev = ic->ic_osdev;
    struct net_device *dev = osdev->netdev;

    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    acfg_hdr_rsp = &acfg_nl->acfg_resp;

    memcpy(acfg_hdr_rsp, acfg_hdr, sizeof(struct acfg_offchan_resp));
    acfg_hdr_rsp->hdr.msg_length = sizeof(struct acfg_offchan_resp);
    if (stat) {
        memcpy(&(acfg_hdr_rsp->stat), stat, sizeof(acfg_offchan_stat_t));
    }

    if (qdf_atomic_dec_and_test(&acfg_nl->in_lock)) {
      qdf_semaphore_release(acfg_nl->sem_lock);
    }
    acfg_net_indicate_event(dev, osdev, acfg_hdr_rsp, ACFG_NETLINK_MSG_RESPONSE, 0, process_pid);

    if (vap != NULL && vap->iv_ic->ic_is_mode_offload) {
        if (vap->iv_opmode == IEEE80211_M_MONITOR &&
            acfg_nl->restore_mon_filter) {
            acfg_nl->restore_mon_filter = 0;
            wlan_set_monitor_filter(ic, ic->ic_os_monrxfilter);
        }
    }
}


/* Callback function for the Completion of Disassoc request frame. */
static void
acfg_frame_tx_compl_handler(wlan_if_t vaphandle, wbuf_t wbuf, void *arg,
                                    u_int8_t *dst_addr, u_int8_t *src_addr,
                                    u_int8_t *bssid, ieee80211_xmit_status *ts)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct acfg_offchan_resp *acfg_resp;
    acfg_netlink_pvt_t *acfg_nl;
    osif_dev *osifp;
    uint32_t id = 0;

    osifp = (osif_dev *)vap->iv_ifp;
    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    acfg_resp = &acfg_nl->acfg_resp;

    /*Drain tx queue if VAP stop is in progress*/
    if (wlan_vdev_is_going_down(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
        acfg_offchan_tx_drain(acfg_nl);
        ucfg_offchan_tx_cancel(vap->vdev_obj);
        OS_SET_TIMER(&acfg_nl->offchan_timer, MAX_OFFCHAN_WAIT);
    }
    wbuf_get_pid(wbuf, &id);

    switch (ts->ts_flags) {
        case 0: // no error
            acfg_resp->status[id].status = ACFG_PKT_STATUS_SUCCESS;
            break;
        case IEEE80211_TX_ERROR:
            acfg_resp->status[id].status = ACFG_PKT_STATUS_ERROR;
            break;
        case IEEE80211_TX_XRETRY:
            acfg_resp->status[id].status = ACFG_PKT_STATUS_XRETRY;
            break;
        default:
            acfg_resp->status[id].status = ACFG_PKT_STATUS_ERROR;
            break;
    }
    acfg_resp->hdr.msg_type = ACFG_PKT_STATUS_SUCCESS;

    if (qdf_atomic_read(&acfg_nl->num_cmpl_pending) > 0) {
        qdf_atomic_dec(&acfg_nl->num_cmpl_pending);
    }

    if (qdf_atomic_read(&acfg_nl->num_cmpl_pending) == 0) {
        /* For home channel, send response here. */
        if (acfg_nl->home_chan) {
            acfg_send_msg_response(vap->iv_ic, acfg_resp, acfg_nl->process_pid, NULL, vap);
            return;
        }
    } else {
        /* Send the next frame */
        acfg_homechan_tx_send(vap, acfg_nl);
    }

    return;
}

static void
acfg_offchan_tx_complete(struct wlan_objmgr_vdev *vdev,
                                struct offchan_tx_status *status,
                                struct offchan_stats *stats)
{
    struct ieee80211vap *vap;
    uint8_t i;
    struct ieee80211com *ic;
    struct acfg_offchan_resp *acfg_resp;
    acfg_netlink_pvt_t *acfg_nl;
    osif_dev *osifp;
    uint32_t id = 0;
    struct ol_ath_softc_net80211 * scn;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if(vap == NULL) {
        return;
    }

    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    osifp = (osif_dev *)vap->iv_ifp;
    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    acfg_resp = &acfg_nl->acfg_resp;

    for (i = 0; i < status->count; i++) {
        id = status->id[i];
        switch (status->status[i]) {
        case OFFCHAN_TX_STATUS_SENT:
        case OFFCHAN_TX_STATUS_SUCCESS:
             acfg_resp->status[id].status = ACFG_PKT_STATUS_SUCCESS;
             break;
        case OFFCHAN_TX_STATUS_DROPPED:
             acfg_resp->status[id].status = ACFG_PKT_STATUS_ERROR;
             break;
        case OFFCHAN_TX_STATUS_XRETRY:
             acfg_resp->status[id].status = ACFG_PKT_STATUS_XRETRY;
             break;
        default:
             acfg_resp->status[id].status = ACFG_PKT_STATUS_ERROR;
             break;
        }
    }
    acfg_resp->hdr.msg_type = ACFG_PKT_STATUS_SUCCESS;

    OS_CANCEL_TIMER(&acfg_nl->offchan_timer);
    acfg_nl->offchan_stat.cycle_count = scn->mib_cycle_cnts.cycle_count;
    acfg_nl->offchan_stat.tx_frame_count = scn->mib_cycle_cnts.tx_frame_count;
    acfg_nl->offchan_stat.rx_frame_count = scn->mib_cycle_cnts.rx_frame_count;
    acfg_nl->offchan_stat.rx_clear_count = scn->mib_cycle_cnts.rx_clear_count;
    acfg_nl->offchan_stat.noise_floor = scn->chan_nf;
    acfg_nl->offchan_stat.dwell_time = stats->dwell_time;
    acfg_nl->offchan_stat.chanswitch_time_htof = stats->chanswitch_time_htof;
    acfg_nl->offchan_stat.chanswitch_time_ftoh = stats->chanswitch_time_ftoh;

    acfg_send_msg_response(vap->iv_ic, acfg_resp, acfg_nl->process_pid,
                           &acfg_nl->offchan_stat, vap);

    ieee80211_ic_offchanscan_clear(vap->iv_ic);
}


static
int acfg_frame_tx_offload(wlan_if_t vaphandle, void *frame)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node *ni;
    wbuf_t wbuf = (wbuf_t)frame;
    int status = 0;

    ni = ieee80211_try_ref_node(vap->iv_bss);
    if (ni) {
        status = ieee80211_send_mgmt(vap,ni, wbuf,true);
    }
    ieee80211_free_node(ni);

    return status;
}

#if QCA_SUPPORT_GPR

/* Sending GPR frame */
static
int acfg_send_gpr(wlan_if_t vaphandle, struct acfg_offchan_tx_hdr *acfg_frame, wbuf_t wbuf)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node *ni;
    int status = 0;
    ni = ieee80211_try_ref_node(vap->iv_bss);
    if (ni) {
        wbuf_set_complete_handler(wbuf, NULL, NULL);
        status = ieee80211_send_mgmt(vap,ni, wbuf,true);
        ieee80211_free_node(ni);
    }

    return status;
}

/* rate check for gpr */
static
int acfg_gpr_rate_check(struct ieee80211vap *vap, struct acfg_offchan_tx_hdr *acfg_frame)
{
    struct ieee80211com *ic;
    wlan_chan_t chan;
    u_int32_t rate = 0;
    int retv = EOK;
    ol_txrx_soc_handle soc_txrx_handle;
    struct wlan_objmgr_psoc *psoc;

    ic = (struct ieee80211com *)vap->iv_ic;
    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

    /*Only support legacy rates for mgmt/data for gratuitous TX*/
    if (acfg_frame->preamble > 1){
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
              "%s(): preamble %d not supported, only 0 , 1 supported\n", __func__, acfg_frame->preamble);
        return -EINVAL;
    }

    if (acfg_frame->mcs > 7){
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
              "%s(): mcs %d not supported, need to be < 8\n", __func__, acfg_frame->mcs);
        return -EINVAL;
    }

    chan = wlan_get_current_channel(vap, true);
    if( IEEE80211_IS_CHAN_5GHZ(chan) && acfg_frame->preamble == 1 ){
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
              "%s(): Invalid rate: CCK rate is not valid for 5G\n", __func__);
        return -EINVAL;
    }

    /* check if basic rate or not for mgmt frame */
    if (acfg_frame->type == ACFG_PKT_TYPE_GPR) {
        rate = cdp_host_stats_get_ratekbps(soc_txrx_handle,
                                           acfg_frame->preamble,
                                           acfg_frame->mcs, 1, 0);
        if (!rate) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                              "%s(): Unable to fetch rate!\n", __func__);
            return -EINVAL;
        }
        if (! ieee80211_rate_is_valid_basic(vap, rate)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                              "%s(): Invalid rate!\n", __func__);
            return -EINVAL;
        }
    }

    return retv;
}


/* configure and return wbuf */
wbuf_t acfg_gpr_add(struct ieee80211vap *vap, struct acfg_offchan_tx_hdr *acfg_frame)
{
    struct ieee80211com *ic;
    struct  ieee80211_frame *wh;
    wbuf_t wbuf = NULL;


    ic = (struct ieee80211com *)vap->iv_ic;

    /* Copy address per vap */
    wh = (struct ieee80211_frame *)(ic->acfg_frame+1);
    IEEE80211_ADDR_COPY(wh->i_addr1, IEEE80211_GET_BCAST_ADDR(vap->iv_ic));
    IEEE80211_ADDR_COPY(wh->i_addr2, vap->iv_myaddr);
    IEEE80211_ADDR_COPY(wh->i_addr3, vap->iv_myaddr);

    /* Alloc and copy the msg into a new buf */
    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_DATA, acfg_frame->length);
    if (wbuf == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                    "%s(): error allocating wbuf!\n", __func__);
        goto next;
    }

    /* The frame starts after the header struct acfg_frame */
    OS_MEMCPY(wbuf_header(wbuf), acfg_frame +1, acfg_frame->length);
    wbuf_set_pktlen(wbuf, acfg_frame->length);

    /* force with NONPAUSE_TID */
    wbuf_set_tid(wbuf, EXT_TID_NONPAUSE);
    /* Set tx ctrl params */
    wbuf_set_tx_rate(wbuf, acfg_frame->nss, acfg_frame->preamble,
            acfg_frame->mcs, ic->ic_he_target);
    wbuf_set_tx_ctrl(wbuf, 0, ATH_TXPOWER_MAX, -1);
#ifdef QCA_SUPPORT_CP_STATS
    vdev_cp_stats_tx_offchan_mgmt_inc(vap->vdev_obj, 1);
#endif

next:
     return wbuf;
}

#endif

int acfg_offchan_tx_add(struct ieee80211vap *vap, int num_frames,
                    struct acfg_offchan_resp * acfg_resp, struct acfg_offchan_tx_hdr *acfg_frame,
                    acfg_netlink_pvt_t *acfg_nl)
{
    struct ieee80211com *ic;
    int i;
    wlan_chan_t chan;
    int status = 1;
    u_int32_t rate = 0;
    wbuf_t wbuf = NULL;
    ol_txrx_soc_handle soc_txrx_handle;
    struct wlan_objmgr_psoc *psoc;
    osif_dev *osifp = (osif_dev *)vap->iv_ifp;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_OFFCHAN, "%s() on VAP %s, tx queue len %d\n", __func__,
                        osifp->netdev->name, qdf_atomic_read(&acfg_nl->num_cmpl_pending));

    ic = (struct ieee80211com *)vap->iv_ic;
    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);

        /* add the frames one-by-one  */
        for(i = 0; i < num_frames; i++) {

            acfg_resp->status[i].id = acfg_frame->id;

            /*Only support legacy rates for mgmt/data for offchan TX*/
            if (acfg_frame->preamble > 1){
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                      "%s(): preamble %d not supported, only 0 , 1 supported\n", __func__, acfg_frame->preamble);
                acfg_resp->status[i].status = ACFG_PKT_STATUS_BAD;
                goto next;
            }

            if (acfg_frame->mcs > 7){
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                      "%s(): mcs %d not supported, need to be < 8\n", __func__, acfg_frame->mcs);
                acfg_resp->status[i].status = ACFG_PKT_STATUS_BAD;
                goto next;
            }

            chan = wlan_get_current_channel(vap, true);
            if( IEEE80211_IS_CHAN_5GHZ(chan) && acfg_frame->preamble == 1 ){
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                      "%s(): Invalid rate: CCK rate is not valid for 5G\n", __func__);
                acfg_resp->status[i].status = ACFG_PKT_STATUS_BAD;
                goto next;
            }

            /* check if basic rate or not for mgmt frame */
            if (acfg_frame->type == ACFG_PKT_TYPE_MGMT) {
                    rate = cdp_host_stats_get_ratekbps(soc_txrx_handle,
                                                       acfg_frame->preamble,
                                                       acfg_frame->mcs, 1, 0);
                    if (!rate) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                                        "%s(): Unable to fetch rate!\n", __func__);
                        goto next;
                    }
                    if (! ieee80211_rate_is_valid_basic(vap, rate)) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                                        "%s(): Invalid rate!\n", __func__);
                        acfg_resp->status[i].status = ACFG_PKT_STATUS_BAD;
                        goto next;
                    }
            }

            /* Alloc and copy the msg into a new buf */
            wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_DATA, acfg_frame->length);
            if (wbuf == NULL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                            "%s(): error allocating wbuf!\n", __func__);
                acfg_resp->status[i].status = ACFG_PKT_STATUS_BAD;
                goto next;
            }

            /* The frame starts after the header struct acfg_frame */
            OS_MEMCPY(wbuf_header(wbuf), acfg_frame +1, acfg_frame->length);
            wbuf_set_pktlen(wbuf, acfg_frame->length);

            /* force with NONPAUSE_TID */
            wbuf_set_tid(wbuf, EXT_TID_NONPAUSE);
            /* Set tx ctrl params */
            wbuf_set_tx_rate(wbuf, acfg_frame->nss, acfg_frame->preamble,
                    acfg_frame->mcs, ic->ic_he_target);
            wbuf_set_tx_ctrl(wbuf, acfg_frame->retry, acfg_frame->power, -1);

            if (acfg_nl->home_chan == 1) {
                  wbuf_set_pid(wbuf, i);
                  qdf_atomic_inc(&acfg_nl->num_cmpl_pending);
            }
            qdf_spin_lock_bh(&acfg_nl->offchan_lock);
            qdf_nbuf_queue_add(&acfg_nl->offchan_list, wbuf);
            qdf_spin_unlock_bh(&acfg_nl->offchan_lock);
            /* offchan TX stats */
            if (acfg_frame->type == ACFG_PKT_TYPE_MGMT) {
#ifdef QCA_SUPPORT_CP_STATS
                vdev_cp_stats_tx_offchan_mgmt_inc(vap->vdev_obj, 1);
#endif
            }else{
#ifdef QCA_SUPPORT_CP_STATS
                vdev_cp_stats_tx_offchan_data_inc(vap->vdev_obj, 1);
#endif
            }
        next:
            /* move to the next frame which follows the current frame */
            acfg_frame = (struct acfg_offchan_tx_hdr *)((uint8_t*)(acfg_frame + 1) + acfg_frame->length);
        }

        return !status;
}

void acfg_homechan_tx_send(struct ieee80211vap *vap, acfg_netlink_pvt_t *acfg_nl)
{
    wbuf_t wbuf = NULL;
    if (acfg_nl == NULL) {
        qdf_print("%s: acfg_nl is NULL",__FUNCTION__);
        return;
    }

    qdf_spin_lock_bh(&acfg_nl->offchan_lock);
    if (!qdf_nbuf_is_queue_empty(&acfg_nl->offchan_list)) {
        wbuf = qdf_nbuf_queue_remove(&(acfg_nl->offchan_list));
        if (wbuf == NULL) {
            qdf_spin_unlock_bh(&acfg_nl->offchan_lock);
            return;
        }
        wbuf_set_complete_handler(wbuf, acfg_frame_tx_compl_handler, NULL);
        qdf_spin_unlock_bh(&acfg_nl->offchan_lock);
        /* send the pkt */
        if (acfg_frame_tx_offload(vap, wbuf)){
            /*when mgmt tx failed, skb free is already called in
              ol_ath_tx_mgmt_wmi_send=>ieee80211_complete_wbuf */
#ifdef QCA_SUPPORT_CP_STATS
            vdev_cp_stats_tx_offchan_fail_inc(vap->vdev_obj, 1);
#endif
        }
    }else{
        qdf_spin_unlock_bh(&acfg_nl->offchan_lock);
    }
}

void acfg_offchan_tx_drain(acfg_netlink_pvt_t *acfg_nl)
{
    wbuf_t wbuf = NULL;

    qdf_spin_lock_bh(&acfg_nl->offchan_lock);
    while(!qdf_nbuf_is_queue_empty(&acfg_nl->offchan_list)) {
        wbuf = qdf_nbuf_queue_remove(&(acfg_nl->offchan_list));
        if (wbuf) {
            if (acfg_nl->home_chan == 1) {
                  qdf_atomic_dec(&acfg_nl->num_cmpl_pending);
            }
            wbuf_free(wbuf);
        }
    }
    qdf_spin_unlock_bh(&acfg_nl->offchan_lock);
}

static
void acfg_offchan_rx_ind(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap;
    struct ieee80211com *ic;
    struct acfg_offchan_resp *acfg_resp;
    acfg_netlink_pvt_t *acfg_nl;
    struct ol_ath_softc_net80211 * scn;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if(vap == NULL) {
        return;
    }

    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    acfg_resp = &acfg_nl->acfg_resp;

    OS_MEMSET(&acfg_nl->offchan_stat, 0, sizeof(acfg_offchan_stat_t));

    acfg_nl->offchan_stat.cycle_count = scn->mib_cycle_cnts.cycle_count;

    acfg_nl->acfg_resp.hdr.msg_type = ACFG_PKT_STATUS_SUCCESS;
}

static
int acfg_offchan_tx_offload(wlan_if_t vaphandle, u_int32_t chan,
                            u_int16_t scan_dur, u_int16_t chan_no,
                            u_int8_t bw_mode, u_int8_t sec_chan_offset,
                            u_int32_t offchan_rx)
{
    struct ieee80211vap *vap = vaphandle;
    struct offchan_tx_req req = { 0 };
    acfg_netlink_pvt_t *acfg_nl;

    /*
     * For off channel TX/RX without active WLAN feature, VAP UP/RUN state
     * is not mandatory.
     */

    acfg_nl = (acfg_netlink_pvt_t *)vap->iv_ic->ic_acfg_handle;

    req.chan = chan;
    req.dwell_time = scan_dur;
    req.wide_scan.bw_mode = bw_mode;
    req.wide_scan.chan_no = chan_no;
    req.wide_scan.sec_chan_offset = sec_chan_offset;
    req.offchan_rx = offchan_rx;
    req.complete_dwell_tx = false;
    req.dequeue_rate = 1;
    req.req_nbuf_ontx_comp = false;
    req.tx_comp = acfg_offchan_tx_complete;
    req.rx_ind = acfg_offchan_rx_ind;
    qdf_nbuf_queue_init(&req.offchan_tx_list);
    qdf_spin_lock_bh(&acfg_nl->offchan_lock);
    qdf_nbuf_queue_append(&req.offchan_tx_list, &acfg_nl->offchan_list);
    qdf_nbuf_queue_init(&acfg_nl->offchan_list);
    qdf_spin_unlock_bh(&acfg_nl->offchan_lock);

    if (ucfg_offchan_tx_request(vap->vdev_obj, &req) != QDF_STATUS_SUCCESS) {
        return -EINVAL;
    }
    ieee80211_ic_offchanscan_set(vap->iv_ic);

    return 0;
}

static struct ieee80211vap *ieee80211_acfg_find_vap(uint8_t *vap_name, struct ieee80211com **ic_ptr)
{
    struct ieee80211com *ic;
    struct ieee80211vap *vap = NULL, *tvap = NULL, *pvap = NULL;
    int i;
    struct ath_softc *sc;
    ol_ath_soc_softc_t *soc;
    osif_dev *osifp;
    int soc_idx;
    struct wlan_objmgr_vdev *vdev;

    if (strlen(vap_name) >= ACFG_MAX_IFNAME) {
      return NULL;
    }

    /* check the ol list first */
    for (soc_idx = 0; soc_idx < ol_num_global_soc; soc_idx++) {
        soc = ol_global_soc[soc_idx];
        if (soc == NULL)
            continue;
        /*
         * Release vdev reference in the caller of this function
         * to avoid deletion of vdev during use.
         */
        vdev = osif_get_vdev_by_ifname(soc->psoc_obj, vap_name,
                                       WLAN_MLME_NB_ID);
        if (vdev != NULL) {
            pvap = wlan_vdev_get_vap(vdev);
            *ic_ptr = wlan_vdev_get_ic(vdev);
            if (pvap != NULL) {
                  goto found;
            }
        }
    }

    /* check the DA list */
    for (i = 0; i < num_global_scn; i ++) {
        sc = ATH_DEV_TO_SC(global_scn[i]->sc_dev);
        ic = (struct ieee80211com *)sc->sc_ieee;
        IEEE80211_COMM_LOCK_BH(ic);
        *ic_ptr = ic;
        if (!TAILQ_EMPTY(&ic->ic_vaps)) {
            TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                osifp = (osif_dev *)vap->iv_ifp;
                if (osifp == NULL) {
                    continue;
                }
                if (!strcmp(osifp->netdev->name, vap_name)) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                            "%s(): DA not supported \n", __func__);
                    IEEE80211_COMM_UNLOCK_BH(ic);
                    goto found;
                }
            }
        }
        IEEE80211_COMM_UNLOCK_BH(ic);
    }
 found:
    return pvap;
}

#if QCA_SUPPORT_GPR
/*
 * Callback function that will be called at the expiration of gpr_timer.
 */
static enum qdf_hrtimer_restart_status ieee80211_send_gratitous_resp_handler(qdf_hrtimer_data_t *arg)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL;
    void *ptr = (void *) arg;
    qdf_hrtimer_data_t *thr = container_of(ptr, qdf_hrtimer_data_t, u);
    wbuf_t  wbuf = NULL;
    qdf_hrtimer_data_t *gpr_gen_timer = NULL;
    ic = container_of(thr, struct ieee80211com, ic_gpr_timer);
    if(!ic || !ic->acfg_frame) {
        return QDF_HRTIMER_NORESTART;
    }

    gpr_gen_timer = &ic->ic_gpr_timer;

    /* Loop through and send GPR for each enabled vap */
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap) {
            /* If AP vap is running and gpr is enabled */
            if (vap->iv_opmode == IEEE80211_M_HOSTAP && vap->iv_gpr_enable && wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
                /* Skip grp for this vap */
                if ((ieee80211_vap_deleted_is_set(vap))) {
                   continue;
                }
                wbuf = acfg_gpr_add(vap,ic->acfg_frame);
                /* Send gratuitous probe response */
                if(wbuf){
                    acfg_send_gpr(vap, ic->acfg_frame, wbuf);
                    ic->ic_gpr_send_count++;
                }
                else {
                    return QDF_HRTIMER_NORESTART;
                }
           }
        }
    }
    /* forward the timer */
    qdf_hrtimer_forward(gpr_gen_timer, qdf_ktime_get(), ic->ic_gpr_period);
    ic->ic_gpr_timer_resize_count++;
    return QDF_HRTIMER_RESTART;
}
#endif

void acfg_process_msg(wbuf_t msg)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap;
    osif_dev *osifp;
    acfg_netlink_pvt_t *acfg_nl;
    struct acfg_offchan_hdr acfg_hdr;
    void *nlh;
    int error = -1;
    wlan_chan_t chan;
    struct acfg_offchan_resp *acfg_resp;
    struct acfg_offchan_tx_hdr *acfg_frame;
    int process_pid, offchan_rx;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct wlan_objmgr_pdev *pdev;
#if QCA_SUPPORT_GPR
    qdf_hrtimer_data_t *gpr_gen_timer = NULL;
    uint32_t total_len;
#endif
    acfg_hdr.msg_length = 0;
    nlh = wbuf_raw_data(msg);
    OS_MEMCPY(&acfg_hdr, OS_NLMSG_DATA(nlh), sizeof(acfg_hdr));

    /* ieee80211_acfg_find_vap() takes reference to vdev.
     * Make sure we release reference before we return from this func.
     */
    vap = ieee80211_acfg_find_vap(acfg_hdr.vap_name, &ic);
    if (vap == NULL || ic == NULL) {
        qdf_err("VAP %s not found or ic is NULL", acfg_hdr.vap_name);
        goto bad;
    }
#if QCA_SUPPORT_GPR
    gpr_gen_timer = &ic->ic_gpr_timer;
#endif

    osifp = (osif_dev *)vap->iv_ifp;
    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;

    if (!acfg_nl || !(acfg_nl->acfg_sock) || !osifp) {
        //Release the reference to vdev which was takes in ieee80211_acfg_find_vap()
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
        qdf_err("%s : %d: socket is not available or osifp is NULL", __func__, __LINE__);
        return;
    }

    acfg_resp = &acfg_nl->acfg_resp;

    /* Init the response here */
    memcpy(acfg_resp->hdr.vap_name, osifp->netdev->name, ACFG_MAX_IFNAME);
    acfg_resp->hdr.msg_type = ACFG_PKT_STATUS_UNKNOWN;
    acfg_resp->hdr.channel = acfg_hdr.channel;
    acfg_resp->hdr.num_frames = acfg_hdr.num_frames;
    memset(acfg_resp->status, 0xff, sizeof(acfg_resp->status));

    /*Validate if it's good time to do offchan ops*/
    if ((wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
        (wlan_vdev_is_going_down(vap->vdev_obj) == QDF_STATUS_SUCCESS)) {
        qdf_print("%s(): VAP is either not yet up or going down...try again", __func__);
        goto bad;
    }
    /*fail if FW recovery in progress*/
    scn = OL_ATH_SOFTC_NET80211(ic);
    if (scn->sc_ic.recovery_in_progress) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                 "%s(): target recovery in progress, return\n", __func__);
        goto bad;
    }

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is null", __func__);
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
        return;
    }

    /*fail if CAC timer is running*/
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
            QDF_STATUS_SUCCESS) {
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
        return;
    }
    if (mlme_dfs_is_ap_cac_timer_running(pdev)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                 "%s(): cac timer still running, return\n", __func__);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
        goto bad;
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);

    /*fail if ioctl CSA's going on*/
    if( ic->ic_flags_ext2 & IEEE80211_FEXT2_CSA_WAIT ){
        goto bad;
    }

    /* save the pid for use during completion */
    process_pid = ((struct nlmsghdr *)nlh)->nlmsg_pid;	/*pid of sending process */
    acfg_nl->process_pid = process_pid;
    if (acfg_hdr.msg_type == ACFG_CMD_CANCEL) {
        if (acfg_nl->home_chan != 0) {
            /*
             * Off channel.
             * Cancel the scan, response is sent during scan complete.
             */
            if (ucfg_offchan_tx_cancel(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
                /* No active  offchan scan in progress. Send response immediately */
                acfg_send_msg_response(vap->iv_ic, acfg_resp, acfg_nl->process_pid, NULL, vap);
            }
            OS_CANCEL_TIMER(&acfg_nl->offchan_timer);
        }
        else {
            /* Home channel */
            /* send response back to app */
            acfg_send_msg_response(vap->iv_ic, acfg_resp, acfg_nl->process_pid, NULL, vap);
        }
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
        return;
    }

    /* Give access to only one app on one radio */
    if(qdf_semaphore_acquire_intr(acfg_nl->sem_lock)){
        /*failed to acquire mutex*/
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                 "%s(): failed to acquire mutex!\n", __func__);
        goto bad;
    }

#if QCA_SUPPORT_GPR
    acfg_frame = (struct acfg_offchan_tx_hdr *) ((char *)OS_NLMSG_DATA(nlh) +
                                                 sizeof(struct acfg_offchan_hdr));
    if ((acfg_hdr.msg_type == ACFG_PKT_TYPE_GPR)) {
        if (ic->ic_gpr_enable == 0 && !acfg_gpr_rate_check(vap,acfg_frame)) {

            total_len = acfg_frame->length + sizeof(struct acfg_offchan_tx_hdr) + sizeof(struct acfg_offchan_hdr)
                                                           + sizeof(struct nlmsghdr);
            if (total_len > GPR_MAX_PAYLOAD) {
                qdf_err("%s %d Bad lenght, allowed length of payload is %d  \n",__func__,__LINE__,GPR_MAX_PAYLOAD);
                wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
                qdf_semaphore_release(acfg_nl->sem_lock);
                return;
            }
            /* Check for previous allocations */
            if (ic->acfg_frame) {
                qdf_mem_free(ic->acfg_frame);
                ic->acfg_frame = NULL;
            }
            ic->acfg_frame  = qdf_mem_malloc(sizeof(struct acfg_offchan_tx_hdr)+ acfg_frame->length);
            if (!ic->acfg_frame) {
                qdf_err("%s %d Memory allocation failed  GPR \n",__func__,__LINE__);
                wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
                qdf_semaphore_release(acfg_nl->sem_lock);
                return;
            }
            OS_MEMCPY(ic->acfg_frame, acfg_frame, sizeof(struct acfg_offchan_tx_hdr)+acfg_frame->length);
            ic->ic_period = acfg_frame->period;
            acfg_nl->vap = vap;
            ic->ic_gpr_period = ms_to_ktime(acfg_frame->period);
            ic->ic_gpr_enable = 1;

            if (!qdf_hrtimer_active(gpr_gen_timer)) {
                qdf_hrtimer_start(gpr_gen_timer, ic->ic_gpr_period, QDF_HRTIMER_MODE_REL);
                ic->ic_gpr_timer_start_count++;

                qdf_err("\n%s %d Periodicity: %d | Current time: %lld | Beacon: %lld | effchantime: %lld | "
                    " Timer expires: %lld | Timer cb: %d | Enqueued: %d \n", \
                    __func__, __LINE__,
                    ic->ic_period, qdf_ktime_to_ns(qdf_ktime_get()), qdf_ktime_to_ns(vap->iv_next_beacon_tstamp),
                    qdf_ktime_to_ns(ic->ic_gpr_period), qdf_ktime_to_ns(qdf_ktime_add(qdf_ktime_get(),
                    qdf_hrtimer_get_remaining(gpr_gen_timer))),
                    qdf_hrtimer_callback_running(gpr_gen_timer), qdf_hrtimer_is_queued(gpr_gen_timer));
            }
            wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
            qdf_semaphore_release(acfg_nl->sem_lock);
            return ;
        }
        if (ic->ic_gpr_enable && !acfg_gpr_rate_check(vap,acfg_frame)) {
            /*update time period*/
            if (qdf_ktime_to_ms(ms_to_ktime(acfg_frame->period)) != qdf_ktime_to_ms(ic->ic_gpr_period)) {
                ic->ic_gpr_period = ms_to_ktime(acfg_frame->period);
                ic->ic_period = acfg_frame->period;
                qdf_err("\nGPR timer period adjustment \n");
            }
        }
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
        qdf_semaphore_release(acfg_nl->sem_lock);
        return;
    }
#endif

    qdf_atomic_set(&acfg_nl->in_lock, 1);

    acfg_frame = (struct acfg_offchan_tx_hdr *) ((char *)OS_NLMSG_DATA(nlh) +
                                                 sizeof(struct acfg_offchan_hdr));
    acfg_nl->vap = vap;
    offchan_rx = acfg_hdr.msg_length ? 0 : 1;

    if (offchan_rx != 1 && vap->iv_opmode == IEEE80211_M_MONITOR) {
        /* offchan TX on mon vap's not allowed */
        goto bad;
    }
    if (vap->iv_ic->ic_is_mode_offload && vap->iv_opmode == IEEE80211_M_MONITOR) {
        acfg_nl->restore_mon_filter = 1;
        wlan_set_monitor_filter(vap->iv_ic, MON_FILTER_ALL_DISABLE);
    }

    /* Check for home channel */
    chan = wlan_get_current_channel(vap, true);
    if ((acfg_hdr.channel == chan->ic_ieee) && (acfg_hdr.msg_length > 0)) {
        /* Home channel */
        acfg_nl->home_chan = 1;
        qdf_atomic_init(&acfg_nl->num_cmpl_pending);

        /* Add the pkts to the queue */
        acfg_offchan_tx_add(vap, acfg_hdr.num_frames, acfg_resp, acfg_frame, acfg_nl);

        /* Send them out */
        if (!qdf_nbuf_is_queue_empty(&acfg_nl->offchan_list)) {
            acfg_homechan_tx_send(vap, acfg_nl);
        } else {
            goto bad;
        }

    } else {
        struct ieee80211_ath_channel *chan;
        /*
         * Off-channel.
         */
        acfg_nl->home_chan = 0;

        /* check channel is supported */
        if ((chan = ieee80211_find_dot11_channel(ic, acfg_hdr.channel, 0, IEEE80211_MODE_AUTO)) == NULL){
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                     "%s(): channel not supported !\n", __func__);
            goto bad;
        }

        /* Add the pkts to the queue */
        acfg_offchan_tx_add(vap, acfg_hdr.num_frames, acfg_resp, acfg_frame, acfg_nl);

        /* set a timeout for this operation. As offchan scan taking
         * long time in firmware, so adding beacon interval as well
         * to set timer*/
        OS_SET_TIMER(&acfg_nl->offchan_timer, MAX_OFFCHAN_WAIT + acfg_hdr.scan_dur + ic->ic_intval);

        /*
         * issue scan and wait for scan events before sending the pkts.
         */
        error = acfg_offchan_tx_offload(vap, chan->ic_freq, acfg_hdr.scan_dur,
                                        acfg_hdr.channel, acfg_hdr.bw_mode,
                                        acfg_hdr.sec_chan_offset, offchan_rx);

        if (error != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                      "%s(): offchan tx cmd failed with error: %d\n", __func__,error);
            acfg_offchan_tx_drain(acfg_nl);
            goto bad;
        }
        /* the response msg is sent to app in the scan event handler */
    }

    wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
    return;

 bad:

    if (ic == NULL) {
       /* This should not happen, atleast when vaps are present */
       qdf_print("%s(): ic is NULL, seems no VAP is present", __func__);

       if (vap) {
           wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
       }
       return;
    }

    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    if (!acfg_nl || !(acfg_nl->acfg_sock)) {
        qdf_print("%s : %d : socket is not available.", __func__, __LINE__);
        if (acfg_nl && qdf_atomic_dec_and_test(&acfg_nl->in_lock)) {
            qdf_semaphore_release(acfg_nl->sem_lock);
        }

        if (vap) {
            wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
        }
        return;
    }

    acfg_resp = &acfg_nl->acfg_resp;
    acfg_resp->hdr.msg_type = ACFG_PKT_STATUS_ERROR;
    /* send an error response msg to the app */
    acfg_send_msg_response(ic, acfg_resp, ((struct nlmsghdr *)nlh)->nlmsg_pid, NULL, vap);

    if (vap) {
        wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_MLME_NB_ID);
    }

    return;
}

static OS_TIMER_FUNC(acfg_offchan_timer)
{
    acfg_netlink_pvt_t *acfg_nl = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL;
    struct acfg_offchan_resp *acfg_resp;
    osif_dev *osifp = NULL;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    acfg_resp = &acfg_nl->acfg_resp;

    vap = acfg_nl->vap;
    if (vap == NULL) {
        goto out;
    }
    acfg_nl->vap = NULL;

    osifp = (osif_dev *)vap->iv_ifp;
    ieee80211_ic_offchanscan_clear(ic);
    acfg_offchan_tx_drain(acfg_nl);

out:
    /* Send the response to the app */
    acfg_resp->hdr.msg_type = ACFG_PKT_STATUS_TIMEOUT;
    acfg_send_msg_response(ic, acfg_resp, acfg_nl->process_pid, &acfg_nl->offchan_stat, vap);
}

void acfg_offchan_cancel(struct ieee80211com *ic)
{
    acfg_netlink_pvt_t *acfg_nl = NULL;
    struct ieee80211vap *vap = NULL;
    struct acfg_offchan_resp *acfg_resp;

    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    acfg_resp = &acfg_nl->acfg_resp;

    vap = acfg_nl->vap;
    if (NULL == vap) {
        return;
    }
    ucfg_offchan_tx_cancel(vap->vdev_obj);
    OS_CANCEL_TIMER(&acfg_nl->offchan_timer);
    acfg_offchan_tx_drain(acfg_nl);

    /* Send the response to the app */
    acfg_resp->hdr.msg_type = ACFG_PKT_STATUS_TIMEOUT;
    acfg_send_msg_response(ic, acfg_resp, acfg_nl->process_pid, NULL, vap);
}
EXPORT_SYMBOL(acfg_offchan_cancel);

int acfg_attach(struct ieee80211com *ic)
{
    acfg_netlink_pvt_t *acfg_nl, *acfg_nl_tmp;
    struct ieee80211com *ic_tmp = NULL;
    ol_ath_soc_softc_t *soc;
    struct ath_softc *sc = NULL;
    osdev_t osdev = ic->ic_osdev;
    int i;
    int soc_idx, scn_idx;
    struct wlan_objmgr_pdev *pdev;

    if (ic->ic_acfg_handle == NULL) {

#if UMAC_SUPPORT_ACFG
        acfg_event_workqueue_init(osdev);
#endif

        ic->ic_acfg_handle = (void *) (OS_MALLOC(
                    ic->ic_osdev,
                    sizeof(acfg_netlink_pvt_t),
                    GFP_KERNEL));
        if (!ic->ic_acfg_handle) {
            qdf_print("%s: Unable to allocate memory ", __func__);
            ic->ic_acfg_handle = NULL;
            return -ENOMEM;
        }

        acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
        osdev->osdev_acfg_handle = acfg_nl;
        OS_MEMSET(acfg_nl, 0, sizeof(*acfg_nl));

        /* alloc & init the mutex */
        acfg_nl->sem_lock = (qdf_semaphore_t *) (OS_MALLOC(
                    ic->ic_osdev,
                    sizeof(qdf_semaphore_t),
                    GFP_KERNEL));
        if (acfg_nl->sem_lock == NULL) {
            qdf_print("%s: Unable to allocate memory ", __func__);
            return -ENOMEM;
        }
        qdf_semaphore_init(acfg_nl->sem_lock);
        qdf_spinlock_create(&acfg_nl->offchan_lock);
        qdf_nbuf_queue_init(&acfg_nl->offchan_list);
        OS_INIT_TIMER(ic->ic_osdev, &acfg_nl->offchan_timer, acfg_offchan_timer, ic, QDF_TIMER_TYPE_WAKE_APPS);
        qdf_atomic_init(&acfg_nl->in_lock);
#if QCA_SUPPORT_GPR
        /* Initialize the Timer */
        qdf_hrtimer_init(&ic->ic_gpr_timer, ieee80211_send_gratitous_resp_handler,
             QDF_CLOCK_MONOTONIC, QDF_HRTIMER_MODE_REL, QDF_CONTEXT_TASKLET);
#endif

        /* check for a socket already created in Direct Attach */
        for (i = 0; i < num_global_scn; i++) {
            sc = ATH_DEV_TO_SC(global_scn[i]->sc_dev);
            if (sc == NULL)
                continue;
            ic_tmp = (struct ieee80211com *)sc->sc_ieee;
            if (ic_tmp == NULL) {
                /* This should not happen, atleast when vaps are present */
                qdf_print("%s(): ic is NULL, seems no VAP is present", __func__);
                return 0;
            }
            if (ic_tmp && ic_tmp->ic_acfg_handle != NULL) {
                acfg_nl_tmp = (acfg_netlink_pvt_t *)ic_tmp->ic_acfg_handle;
                if (acfg_nl_tmp->acfg_sock != NULL) {
                    acfg_nl->acfg_sock = acfg_nl_tmp->acfg_sock;
                    qdf_info("%s: using existing sock %pK", __func__, acfg_nl->acfg_sock);
                    return 0;
                }
            }
        }
        /* check for a socket already created in offload */
        for (soc_idx = 0; soc_idx < GLOBAL_SOC_SIZE; soc_idx++) {
            soc = ol_global_soc[soc_idx];
            if (soc == NULL)
                continue;
            for (scn_idx = 0; scn_idx < WLAN_UMAC_MAX_PDEVS; scn_idx++) {
                pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, scn_idx, WLAN_MLME_NB_ID);
                if (pdev == NULL) {
                     qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
                     continue;
                }

                ic_tmp = wlan_pdev_get_mlme_ext_obj(pdev);

                if (ic_tmp && ic_tmp->ic_acfg_handle != NULL) {
                    acfg_nl_tmp = (acfg_netlink_pvt_t *)ic_tmp->ic_acfg_handle;
                    if (acfg_nl_tmp->acfg_sock != NULL) {
                        acfg_nl->acfg_sock = acfg_nl_tmp->acfg_sock;
                        qdf_info("%s: Offload using existing sock %pK", __func__, acfg_nl->acfg_sock);
                        printk("%s: Offload using existing sock %pK\n", __func__, acfg_nl->acfg_sock);
                        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                        return 0;
                    }
                }
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            }
        }

        if (ic->ic_is_mode_offload(ic)) {
            soc = ((struct ol_ath_softc_net80211 *) ic)->soc;
            for (scn_idx = 0; scn_idx < wlan_psoc_get_pdev_count(soc->psoc_obj); scn_idx++) {
                pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, scn_idx, WLAN_MLME_NB_ID);
                if (pdev == NULL) {
                     qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
                     continue;
                }

                ic_tmp = wlan_pdev_get_mlme_ext_obj(pdev);

                if (ic_tmp && ic_tmp->ic_acfg_handle != NULL) {
                    acfg_nl_tmp = (acfg_netlink_pvt_t *)ic_tmp->ic_acfg_handle;
                    if (acfg_nl_tmp->acfg_sock != NULL) {
                        acfg_nl->acfg_sock = acfg_nl_tmp->acfg_sock;
                        qdf_info("Offload using existing sock %pK", acfg_nl->acfg_sock);
                        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                        return 0;
                    }
                }
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            }
        }

        /*
         * create a new socket for the first time.
         * Note: This is common for all radios
         */
        if (acfg_nl->acfg_sock == NULL) {
            acfg_nl->acfg_sock = OS_NETLINK_CREATE(
                    NETLINK_ACFG,
                    1,
                    (void *)acfg_process_msg,
                    THIS_MODULE);

            if (acfg_nl->acfg_sock == NULL) {
                qdf_print("\n%s:socket creation has failed", __func__);
            } else {
                qdf_info("%s: %d: Netlink socket created:%pK",
                        __FUNCTION__, __LINE__,
                        acfg_nl->acfg_sock);
            }
        }
    }

    return 0;
}

void acfg_detach(struct ieee80211com *ic)
{
    acfg_netlink_pvt_t *acfg_nl;
    osdev_t osdev = ic->ic_osdev;

    acfg_event_workqueue_delete(osdev);

    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    if (acfg_nl && acfg_nl->acfg_sock) {
        if (acfg_nl->sem_lock) {
            OS_FREE(acfg_nl->sem_lock);
            acfg_nl->sem_lock = NULL;
        }

        if (((num_global_scn == 1) && (ol_num_global_soc == 0)) ||
            ((num_global_scn == 0) && (ol_num_global_soc == 1) &&
            (ic->ic_get_pdev_idx(ic) == 0))) {
            /*Free the netlink sock if it's the last scn*/
            OS_SOCKET_RELEASE(acfg_nl->acfg_sock);
            qdf_info("%s Netlink socket released", __func__);
        }
        acfg_nl->acfg_sock = NULL;
    }
    if (acfg_nl) {
        qdf_spinlock_destroy(&acfg_nl->offchan_lock);
        OS_FREE_TIMER(&acfg_nl->offchan_timer);

        OS_FREE(acfg_nl);
        ic->ic_acfg_handle = NULL;
    }
}

void acfg_clean(struct ieee80211com *ic)
{
    acfg_netlink_pvt_t *acfg_nl;
    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;

    if (acfg_nl) {
        OS_CANCEL_TIMER(&acfg_nl->offchan_timer);
    }
}
#endif

/*
* func to return diagnostics status based on elapsed time
*/
static uint32_t
acfg_diag_get_status(int64_t sec)
{
    uint32_t status = 0;

    if(sec <= WAIT_15_SEC)
        status = ACFG_DIAGNOSTICS_DOWN;
    else if(sec > WAIT_15_SEC && sec <= WAIT_1_HR)
        status = ACFG_DIAGNOSTICS_DOWN_15_SEC;
    else
        status = ACFG_DIAGNOSTICS_DOWN_1_HR;

    return status;
}

/*
* func to cleanup diagnostics queues
*/
static void
acfg_diag_freeq(acfg_diag_pvt_t *diag)
{
    acfg_diag_stainfo_t *sta_info = NULL, *tsta_info = NULL;

    if (!TAILQ_EMPTY(&diag->curr_stas)) {
            TAILQ_FOREACH_SAFE(sta_info, &diag->curr_stas, next, tsta_info) {
                TAILQ_REMOVE(&diag->curr_stas, sta_info, next);
                OS_FREE(sta_info);
            }
        }
        sta_info = NULL;
        tsta_info = NULL;
        if (!TAILQ_EMPTY(&diag->prev_stas)) {
            TAILQ_FOREACH_SAFE(sta_info, &diag->prev_stas, next, tsta_info) {
                TAILQ_REMOVE(&diag->prev_stas, sta_info, next);
                OS_FREE(sta_info);
            }
        }
}

/*
* func to add a specific node into queue for monitoring
* purpose based on its type.
*/
static void
acfg_diag_add_sta_info(acfg_diag_pvt_t *diag)
{
    acfg_diag_stainfo_t *curr_stainfo = NULL, *tcurr_stainfo = NULL;

    TAILQ_FOREACH_SAFE(curr_stainfo, &diag->curr_stas, next, tcurr_stainfo) {
    if((curr_stainfo->diag.type == ACFG_DIAGNOSTICS_WARNING) ||
         (curr_stainfo->diag.type == ACFG_DIAGNOSTICS_ERROR)) {
        curr_stainfo->notify = 1;
        curr_stainfo->diag.status = ACFG_DIAGNOSTICS_DOWN;

        TAILQ_REMOVE(&diag->curr_stas, curr_stainfo, next);
        TAILQ_INSERT_TAIL(&diag->prev_stas, curr_stainfo, next);
      } else {
        TAILQ_REMOVE(&diag->curr_stas, curr_stainfo, next);
        OS_FREE(curr_stainfo);
      }
    }

    return;
}

/*
* func to update the various diagnostics fields of a node
* which is under monitor.
*/
static void
acfg_diag_update_sta_info(acfg_diag_stainfo_t *curr_stainfo,
                          acfg_diag_stainfo_t *prev_stainfo)
{
    uint32_t type = 0;
    uint32_t status = 0;
    int64_t sec = 0;

    /* Update the necessary fields.*/
    OS_MEMCPY(prev_stainfo->diag.tstamp, &curr_stainfo->diag.tstamp,
                  sizeof(curr_stainfo->diag.tstamp));
    OS_MEMCPY(prev_stainfo->diag.mode, &curr_stainfo->diag.mode,
                  sizeof(curr_stainfo->diag.mode));
    prev_stainfo->diag.data_rate_threshold = curr_stainfo->diag.data_rate_threshold;
    prev_stainfo->diag.power_level = curr_stainfo->diag.power_level;
    prev_stainfo->diag.rssi = curr_stainfo->diag.rssi;
    prev_stainfo->diag.channel_index = curr_stainfo->diag.channel_index;
    prev_stainfo->diag.tx_data_rate = curr_stainfo->diag.tx_data_rate;
    /* ratecode is sometimes 0 hence nss as well,when mgmt frames are xmited
     * avoid update is such cases */
    if (curr_stainfo->diag.nss) {
        prev_stainfo->diag.mcs_index = curr_stainfo->diag.mcs_index;
        prev_stainfo->diag.chan_width = curr_stainfo->diag.chan_width;
        prev_stainfo->diag.sgi = curr_stainfo->diag.sgi;
        prev_stainfo->diag.nss = curr_stainfo->diag.nss;
    }

    /* Update the type,status if necessary and notify */
    type = curr_stainfo->diag.type;
    switch (type) {
        case ACFG_DIAGNOSTICS_WARNING:
            if(prev_stainfo->diag.type == ACFG_DIAGNOSTICS_WARNING) {
                sec = curr_stainfo->seconds - prev_stainfo->seconds;
                status = acfg_diag_get_status(sec);
                if(status != prev_stainfo->diag.status) {
                    prev_stainfo->diag.status = status;
                    prev_stainfo->notify = 1;
                }
            } else if (prev_stainfo->diag.type == ACFG_DIAGNOSTICS_ERROR) {
                prev_stainfo->diag.type = curr_stainfo->diag.type;
                prev_stainfo->diag.status = ACFG_DIAGNOSTICS_DOWN;
                prev_stainfo->notify = 1;
                prev_stainfo->seconds = curr_stainfo->seconds;
            }
            break;

        case ACFG_DIAGNOSTICS_ERROR:
            if(prev_stainfo->diag.type == ACFG_DIAGNOSTICS_WARNING) {
                prev_stainfo->diag.type = curr_stainfo->diag.type;
                prev_stainfo->diag.status = ACFG_DIAGNOSTICS_DOWN;
                prev_stainfo->notify = 1;
                prev_stainfo->seconds = curr_stainfo->seconds;
            } else if (prev_stainfo->diag.type == ACFG_DIAGNOSTICS_ERROR) {
                sec = curr_stainfo->seconds - prev_stainfo->seconds;
                status = acfg_diag_get_status(sec);
                if(status != prev_stainfo->diag.status) {
                    prev_stainfo->diag.status = status;
                    prev_stainfo->notify = 1;
                }
            }
            break;

        case ACFG_DIAGNOSTICS_NORMAL:
            prev_stainfo->diag.type = curr_stainfo->diag.type;
            prev_stainfo->diag.status = ACFG_DIAGNOSTICS_UP;
            prev_stainfo->notify = 1;
            prev_stainfo->seconds = curr_stainfo->seconds;
            break;
    }

    return;
}

/*
* func to send a netlink event to upper layer.
*/
static void
acfg_diag_notify(acfg_diag_pvt_t *diag)
{
    osif_dev *osifp = NULL;
    osdev_t  osdev = NULL;
    struct net_device *dev = NULL;
    acfg_os_event_t *event = NULL;
    acfg_diag_event_t *diag_event = NULL;
    acfg_diag_stainfo_t *prev_stainfo = NULL, *tprev_stainfo = NULL;

    TAILQ_FOREACH_SAFE(prev_stainfo, &diag->prev_stas, next, tprev_stainfo) {
        if (prev_stainfo->notify) {
            osifp = (osif_dev *)prev_stainfo->vap->iv_ifp;
            dev = osifp->netdev;
            osdev = osifp->os_handle;

            event = NULL;
            event = (acfg_os_event_t *)qdf_mem_malloc_atomic(sizeof(acfg_os_event_t));
            if (event == NULL)
                break;
            event->id = ACFG_EV_DIAGNOSTICS;
            diag_event = (acfg_diag_event_t *)&event->data;
            OS_MEMCPY(diag_event, &prev_stainfo->diag, sizeof(acfg_diag_event_t));

            acfg_net_indicate_event(dev, osdev, event, ACFG_NETLINK_SENT_EVENT, 0, 0);
            qdf_mem_free(event);

            /* This sta got better hence the indication,
             * No need to track this sta anymore*/
            if (prev_stainfo->diag.status == ACFG_DIAGNOSTICS_UP) {
                TAILQ_REMOVE(&diag->prev_stas, prev_stainfo, next);
                OS_FREE(prev_stainfo);
            }
        }
    }

    return;
}

/*
* func to get statistics about the connected nodes.
*/
static void
acfg_diag_get_sta_stats(void *ptr, wlan_node_t node)
{
    acfg_diag_pvt_t *diag = NULL;
    acfg_diag_stainfo_t *sta_info = NULL;
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = (struct ieee80211com *)ptr;
    uint8_t *macaddr = NULL;
    ieee80211_rate_info rinfo;
    wlan_rssi_info rssi_info;
    struct tm tm;
    struct timeval time;
    uint16_t mode;
    wlan_chan_t chan;
    uint32_t warn_threshold, err_threshold;
    struct wlan_objmgr_psoc *psoc = NULL;

    vap = node->ni_vap;
    warn_threshold = vap->iv_diag_warn_threshold;
    err_threshold = vap->iv_diag_err_threshold;

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);

    diag = (acfg_diag_pvt_t *)ic->ic_diag_handle;
    /* During breif moment from assoc/auth state to connected state
     * data rate and ratecode will be 0 as no data pkts are exchnged during this phase.
     * So avoid adding node during this period */
    wlan_node_txrate_info(node, &rinfo);
    if (rinfo.rate && rinfo.mcs) {
        sta_info = (acfg_diag_stainfo_t *) (OS_MALLOC(
                       ic->ic_osdev,
                       sizeof(acfg_diag_stainfo_t),
                       GFP_ATOMIC));
        if(sta_info) {
            OS_MEMSET(sta_info, 0, sizeof(acfg_diag_stainfo_t));

            /* Get type of event and corresponding threshold */
            sta_info->vap = vap;
            sta_info->diag.tx_data_rate = RATE_KBPS_TO_MBPS(rinfo.rate);
            if (sta_info->diag.tx_data_rate < err_threshold) {
                sta_info->diag.type = ACFG_DIAGNOSTICS_ERROR;
                sta_info->diag.data_rate_threshold = err_threshold;
            } else if (sta_info->diag.tx_data_rate < warn_threshold) {
                sta_info->diag.type = ACFG_DIAGNOSTICS_WARNING;
                sta_info->diag.data_rate_threshold = warn_threshold;
            } else
                sta_info->diag.type = ACFG_DIAGNOSTICS_NORMAL;

            /* Get MAC addr */
            macaddr = wlan_node_getmacaddr(node);
            IEEE80211_ADDR_COPY(sta_info->diag.macaddr, macaddr);

            /* Get tx power level */
            sta_info->diag.power_level = wlan_node_get_last_txpower(node);

            /* Get Rssi */
            if (wlan_node_getrssi(node, &rssi_info, WLAN_RSSI_RX) == 0) {
                sta_info->diag.rssi = rssi_info.avg_rssi;
            }

            /* Get mode */
            mode = wlan_node_get_mode(node);
            snprintf(sta_info->diag.mode, sizeof(sta_info->diag.mode), "%s",
                     (mode < 22) ? (char *)phymode_strings[mode] : "IEEE80211_MODE_11B");

            /* Get chan index,assuming chan index as chan num */
            chan = wlan_node_get_chan(node);
            if (chan != IEEE80211_CHAN_ANYC)
                sta_info->diag.channel_index = chan->ic_ieee;

            /* Get timestamp */
            do_gettimeofday(&time);
            sta_info->seconds = time.tv_sec;
            time_to_tm(time.tv_sec, 0, &tm);
            snprintf(sta_info->diag.tstamp,sizeof(sta_info->diag.tstamp),
                        "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

            /* Get airtime */
#if QCA_AIRTIME_FAIRNESS
            if (ucfg_atf_get_mode(psoc))
                sta_info->diag.airtime = ucfg_atf_get_peer_airtime(node->peer_obj);
            else
                sta_info->diag.airtime = 0xFF; /* assign some large val if not atfmode */
#endif

            /* steps to get mcs, chanwidth, sgi and nss differ
             * in offload and direcet attach mode*/
            if (ic->ic_is_mode_offload(ic)) {
                /* Offload mode */
                {
                    if (IS_HW_RATECODE_HT(rinfo.mcs) || IS_HW_RATECODE_VHT(rinfo.mcs)) {
                        sta_info->diag.nss = AR600P_GET_HW_RATECODE_NSS(rinfo.mcs);
                        sta_info->diag.mcs_index = AR600P_GET_HW_RATECODE_RATE(rinfo.mcs);
                        if (IS_HW_RATECODE_HT(rinfo.mcs)) {
                            sta_info->diag.mcs_index = VHT_2_HT_MCS(sta_info->diag.mcs_index,
                                                           sta_info->diag.nss);
                        }

                        sta_info->diag.nss++; /* nss starts from 0 */
                        if (rinfo.flags & WHAL_RC_FLAG_SGI)
                            sta_info->diag.sgi = 1;
                        else
                            sta_info->diag.sgi = 0;
                        if (rinfo.flags & WHAL_RC_FLAG_40MHZ)
                            sta_info->diag.chan_width = 40;
                        else if (rinfo.flags & WHAL_RC_FLAG_80MHZ)
                            sta_info->diag.chan_width = 80;
                        else if (rinfo.flags & WHAL_RC_FLAG_160MHZ)
                            sta_info->diag.chan_width = 160;
                        else
                            sta_info->diag.chan_width = 20;
                    } else {
                            /* Legacy rate
                             * Set mcs index to a large val */
                            sta_info->diag.mcs_index = 0xFF;
                            sta_info->diag.nss  = 1;
                            sta_info->diag.sgi = 0;
                            sta_info->diag.chan_width = 20;
                    }
                }
            }
            else {
                /* Direct Attach mode */
                {
                    if (rinfo.mcs & HT_RATE_CODE) {
                        sta_info->diag.mcs_index = HT_RC_2_MCS(rinfo.mcs);
                        sta_info->diag.nss = HT_RC_2_STREAMS(rinfo.mcs);
                        if (rinfo.flags & ATH_RC_SGI_FLAG)
                            sta_info->diag.sgi = 1;
                        else
                            sta_info->diag.sgi = 0;
                        if (rinfo.flags & ATH_RC_CW40_FLAG)
                            sta_info->diag.chan_width = 40;
                        else
                            sta_info->diag.chan_width = 20;
	            } else {
                        /* Legacy rate
                         * Set mcs index to a large val */
                        sta_info->diag.mcs_index = 0xFF;
                        sta_info->diag.nss  = 1;
                        sta_info->diag.sgi = 0;
                        sta_info->diag.chan_width = 20;
                    }
                }
            }
            TAILQ_INSERT_TAIL(&diag->curr_stas, sta_info, next);
        }
    }
}

/*
* func to monitor node stats and send notification to app if required.
*/
static void
acfg_diag_monitor_sta_stats(acfg_diag_pvt_t *diag)
{
    acfg_diag_stainfo_t *curr_stainfo = NULL, *tcurr_stainfo = NULL;
    acfg_diag_stainfo_t *prev_stainfo = NULL, *tprev_stainfo = NULL;
    uint8_t  found = 0;

    if (!TAILQ_EMPTY(&diag->curr_stas)) {
        if (TAILQ_EMPTY(&diag->prev_stas)) {
            /* No prev list, first time entry.
	     * Insert stainfo of interest into prevlist*/
            acfg_diag_add_sta_info(diag);

        } else {
            /* Try to find a matching entry between curr and prev list.
             * If found update prev entry with new data.*/
            TAILQ_FOREACH_SAFE(prev_stainfo, &diag->prev_stas, next, tprev_stainfo) {
                found = 0;
                prev_stainfo->notify = 0;
                TAILQ_FOREACH_SAFE(curr_stainfo, &diag->curr_stas, next, tcurr_stainfo) {
                    if(OS_MEMCMP(prev_stainfo->diag.macaddr,
                                   curr_stainfo->diag.macaddr, ACFG_MACADDR_LEN) == 0) {
                        found = 1;
                        break;
                    }
                }
                /* Match found. Now update the prev entry based on curr information */
                if(found) {
                    acfg_diag_update_sta_info(curr_stainfo, prev_stainfo);
                    /* prev entry has been updated , we can delete the curr entry now */
                    TAILQ_REMOVE(&diag->curr_stas, curr_stainfo, next);
                    OS_FREE(curr_stainfo);
                 } else {
                    /* No match found. Sta not associated anymore remove prev entry */
                    TAILQ_REMOVE(&diag->prev_stas, prev_stainfo, next);
                    OS_FREE(prev_stainfo);
                }
            }
            /* Now check if any new entry in curr list that needs tracking.
             * If so add them to the prev list*/
            if (!TAILQ_EMPTY(&diag->curr_stas)) {
                acfg_diag_add_sta_info(diag);
            }
        }
        /* Send notification to application*/
        acfg_diag_notify(diag);
    } else {
        /*No sta to monitor free the prev list if exist*/
        if (!TAILQ_EMPTY(&diag->prev_stas)) {
            TAILQ_FOREACH_SAFE(prev_stainfo, &diag->prev_stas, next, tprev_stainfo) {
                TAILQ_REMOVE(&diag->prev_stas, prev_stainfo, next);
                OS_FREE(prev_stainfo);
            }
        }
    }

    return;

}

/*
* Diagnositcs timer function.
*/
static OS_TIMER_FUNC(acfg_diag_timer)
{
    acfg_diag_pvt_t *diag = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL, *tvap = NULL;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    spin_lock_dpc(&(ic)->ic_diag_lock);
    diag = (acfg_diag_pvt_t *)ic->ic_diag_handle;
    if (diag) {
        if (ic->ic_diag_enable) {
            if (!TAILQ_EMPTY(&ic->ic_vaps)) {
                TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                    if (vap->iv_opmode == IEEE80211_M_HOSTAP)
                        wlan_iterate_station_list(vap, acfg_diag_get_sta_stats, ic);
                }
            }
            acfg_diag_monitor_sta_stats(diag);
            OS_SET_TIMER(&diag->diag_timer, MAX_DIAGNOSTICS_WAIT);
        } else {
            acfg_diag_freeq(diag);
        }
    }
    spin_unlock_dpc(&(ic)->ic_diag_lock);

}

/*
* func to allocate and initialise resource for diagnostics.
*/
int acfg_diag_attach(struct ieee80211com *ic)
{
    acfg_diag_pvt_t *diag = NULL;

    /* allocate necessary resources */
    if (ic->ic_diag_handle == NULL) {
        ic->ic_diag_handle = (acfg_diag_pvt_t *) (OS_MALLOC(
                                   ic->ic_osdev,
                                   sizeof(acfg_diag_pvt_t),
                                   GFP_KERNEL));
        if(!ic->ic_diag_handle) {
            qdf_print("%s: Unable to allocate memory ", __func__);
            ic->ic_diag_handle = NULL;
            return -ENOMEM;
        }

        diag = (acfg_diag_pvt_t *)ic->ic_diag_handle;
        OS_MEMSET(diag, 0, sizeof(acfg_diag_pvt_t));
        OS_INIT_TIMER(ic->ic_osdev, &diag->diag_timer, acfg_diag_timer, ic, QDF_TIMER_TYPE_WAKE_APPS);
        TAILQ_INIT(&diag->curr_stas);
        TAILQ_INIT(&diag->prev_stas);
    }
    spin_lock_init(&ic->ic_diag_lock);

    return 0;
}

/*
* func to destroy allocated resources for diagnostics.
*/
void acfg_diag_detach(struct ieee80211com *ic)
{
    acfg_diag_pvt_t *diag = NULL;

    /* free the resources */
    spin_lock_dpc(&(ic)->ic_diag_lock);
    diag = (acfg_diag_pvt_t *)ic->ic_diag_handle;

    if (diag) {
        OS_FREE_TIMER(&diag->diag_timer);
        acfg_diag_freeq(diag);
        OS_FREE(diag);
        ic->ic_diag_enable = 0;
        ic->ic_diag_handle = NULL;
    }
    spin_unlock_dpc(&(ic)->ic_diag_lock);

}
