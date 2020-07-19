/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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
/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/version.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <qdf_net.h>

#include <acfg_api_types.h>

#include <qdf_mem.h>
#include <qdf_util.h>
#include <if_net.h>

#include <qdf_trace.h>

#include <wcfg.h>
#include <osif_net_offload_cfg.h>

/* Generic Prototype declaration macro */
#define PROTO_WIOCTL(name)                                  \
		static uint32_t                                       \
			__wioctl_##name(__qdf_softc_t   *sc, acfg_data_t  *data);





typedef uint32_t (*__wioctl_fn_t)(__qdf_softc_t  *sc, acfg_data_t *data);

#define sc2wifi_cfg(_sc)        ((atd_cfg_wifi_t *)((_sc)->cfg_api))
#define sc2vap_cfg(_sc)         ((atd_cfg_vap_t *)((_sc)->cfg_api))

/* Prototypes */
PROTO_WIOCTL(create_vap);
PROTO_WIOCTL(set_ssid);
PROTO_WIOCTL(get_ssid);
PROTO_WIOCTL(set_testmode);
PROTO_WIOCTL(get_testmode);
PROTO_WIOCTL(get_rssi);
PROTO_WIOCTL(get_custdata);
PROTO_WIOCTL(set_channel);
PROTO_WIOCTL(get_channel);
PROTO_WIOCTL(delete_vap);
PROTO_WIOCTL(set_opmode);
PROTO_WIOCTL(get_opmode);
PROTO_WIOCTL(set_freq);
PROTO_WIOCTL(get_freq);
PROTO_WIOCTL(get_wireless_name);
PROTO_WIOCTL(set_rts);
PROTO_WIOCTL(get_rts);
PROTO_WIOCTL(set_frag);
PROTO_WIOCTL(get_frag);
PROTO_WIOCTL(set_txpow);
PROTO_WIOCTL(get_txpow);
PROTO_WIOCTL(set_ap);
PROTO_WIOCTL(get_ap);
PROTO_WIOCTL(get_range);
PROTO_WIOCTL(set_encode);
PROTO_WIOCTL(get_encode);
PROTO_WIOCTL(set_rate);
PROTO_WIOCTL(get_rate);
PROTO_WIOCTL(set_powmgmt);
PROTO_WIOCTL(get_powmgmt);
PROTO_WIOCTL(set_reg);
PROTO_WIOCTL(get_reg);

PROTO_WIOCTL(set_vap_param);
PROTO_WIOCTL(get_vap_param);
PROTO_WIOCTL(set_vap_vendor_param);
PROTO_WIOCTL(get_vap_vendor_param);
PROTO_WIOCTL(set_vap_wmmparams);
PROTO_WIOCTL(get_vap_wmmparams);
PROTO_WIOCTL(set_radio_param);
PROTO_WIOCTL(get_radio_param);
PROTO_WIOCTL(set_chmode);
PROTO_WIOCTL(get_chmode);
PROTO_WIOCTL(set_scan);
PROTO_WIOCTL(get_scan_results);
PROTO_WIOCTL(get_ath_stats);
PROTO_WIOCTL(clr_ath_stats);
PROTO_WIOCTL(get_stats);
PROTO_WIOCTL(set_phymode);
PROTO_WIOCTL(get_phymode);
PROTO_WIOCTL(get_assoc_sta_info);
PROTO_WIOCTL(set_optie);
PROTO_WIOCTL(get_chan_info);
PROTO_WIOCTL(get_chan_list);
PROTO_WIOCTL(get_mac_address);
PROTO_WIOCTL(set_vap_p2p_param);
PROTO_WIOCTL(get_vap_p2p_param);
PROTO_WIOCTL(set_hwaddr);
/* tx99tool */
PROTO_WIOCTL(tx99tool);
/* nawds config */
PROTO_WIOCTL(config_nawds);
/* security handlers */
PROTO_WIOCTL(wsupp_init);
PROTO_WIOCTL(wsupp_fini);
PROTO_WIOCTL(wsupp_if_add);
PROTO_WIOCTL(wsupp_if_remove);
PROTO_WIOCTL(wsupp_nw_create);
PROTO_WIOCTL(wsupp_nw_delete);
PROTO_WIOCTL(wsupp_nw_set);
PROTO_WIOCTL(wsupp_nw_get);
PROTO_WIOCTL(wsupp_nw_list);
PROTO_WIOCTL(wsupp_wps_req);
PROTO_WIOCTL(wsupp_set);
PROTO_WIOCTL(doth_chswitch);
PROTO_WIOCTL(addmac);
PROTO_WIOCTL(delmac);
PROTO_WIOCTL(set_mlme);
PROTO_WIOCTL(send_mgmt);
PROTO_WIOCTL(set_acparams);
PROTO_WIOCTL(set_filterframe);
PROTO_WIOCTL(set_appiebuf);
PROTO_WIOCTL(set_key);
PROTO_WIOCTL(del_key);
PROTO_WIOCTL(kickmac);
PROTO_WIOCTL(dbgreq);
PROTO_WIOCTL(acl_addmac);
PROTO_WIOCTL(acl_delmac);
PROTO_WIOCTL(acl_getmac);
PROTO_WIOCTL(get_wlan_profile);
PROTO_WIOCTL(is_offload_vap);
PROTO_WIOCTL(phyerr);
PROTO_WIOCTL(set_chn_widthswitch);
PROTO_WIOCTL(set_country);
PROTO_WIOCTL(set_atf_addssid);
PROTO_WIOCTL(set_atf_delssid);
PROTO_WIOCTL(set_atf_addsta);
PROTO_WIOCTL(set_atf_delsta);


#define REQ_NUM(x)     [x]
#define WIOCTL_SW_SZ    ACFG_REQ_LAST_VAP
/**
 * @brief In the order of the ACFG_REQ enum
 */
const __wioctl_fn_t   __wioctl_sw[] = {
	/* WIFI requests */
	REQ_NUM(ACFG_REQ_FIRST_WIFI)        = 0, /* First WIFI entry */
	REQ_NUM(ACFG_REQ_CREATE_VAP)        = __wioctl_create_vap,
	REQ_NUM(ACFG_REQ_SET_RADIO_PARAM)   = __wioctl_set_radio_param,
	REQ_NUM(ACFG_REQ_GET_RADIO_PARAM)   = __wioctl_get_radio_param,
	REQ_NUM(ACFG_REQ_SET_REG)           = __wioctl_set_reg,
	REQ_NUM(ACFG_REQ_GET_REG)           = __wioctl_get_reg,
	REQ_NUM(ACFG_REQ_TX99TOOL)          = __wioctl_tx99tool,
	REQ_NUM(ACFG_REQ_SET_HW_ADDR)       = __wioctl_set_hwaddr,
	REQ_NUM(ACFG_REQ_GET_ATHSTATS)      = __wioctl_get_ath_stats,
	REQ_NUM(ACFG_REQ_CLR_ATHSTATS)      = __wioctl_clr_ath_stats,
	REQ_NUM(ACFG_REQ_GET_PROFILE)	= __wioctl_get_wlan_profile,
	REQ_NUM(ACFG_REQ_PHYERR)            = __wioctl_phyerr,
	REQ_NUM(ACFG_REQ_SET_COUNTRY)       = __wioctl_set_country,
	/* <add here> */
	REQ_NUM(ACFG_REQ_LAST_WIFI)         = 0, /* Last WIFI entry */

	/* VAP requests */
	REQ_NUM(ACFG_REQ_FIRST_VAP)         = 0, /* first VAP entry */
	REQ_NUM(ACFG_REQ_SET_SSID)          = __wioctl_set_ssid,
	REQ_NUM(ACFG_REQ_GET_SSID)          = __wioctl_get_ssid,
	REQ_NUM(ACFG_REQ_SET_CHANNEL)       = __wioctl_set_channel,
	REQ_NUM(ACFG_REQ_GET_CHANNEL)       = __wioctl_get_channel,
	REQ_NUM(ACFG_REQ_DELETE_VAP)        = __wioctl_delete_vap,
	REQ_NUM(ACFG_REQ_SET_OPMODE)        = __wioctl_set_opmode,
	REQ_NUM(ACFG_REQ_GET_OPMODE)        = __wioctl_get_opmode,
	REQ_NUM(ACFG_REQ_SET_FREQUENCY)     = __wioctl_set_freq,
	REQ_NUM(ACFG_REQ_GET_FREQUENCY)     = __wioctl_get_freq,
	REQ_NUM(ACFG_REQ_GET_NAME)          = __wioctl_get_wireless_name,
	REQ_NUM(ACFG_REQ_SET_RTS)           = __wioctl_set_rts,
	REQ_NUM(ACFG_REQ_GET_RTS)           = __wioctl_get_rts,
	REQ_NUM(ACFG_REQ_SET_FRAG)          = __wioctl_set_frag,
	REQ_NUM(ACFG_REQ_GET_FRAG)          = __wioctl_get_frag,
	REQ_NUM(ACFG_REQ_SET_TXPOW)         = __wioctl_set_txpow,
	REQ_NUM(ACFG_REQ_GET_TXPOW)         = __wioctl_get_txpow,
	REQ_NUM(ACFG_REQ_SET_AP)            = __wioctl_set_ap,
	REQ_NUM(ACFG_REQ_GET_AP)            = __wioctl_get_ap,
	REQ_NUM(ACFG_REQ_GET_RANGE)         = __wioctl_get_range,
	REQ_NUM(ACFG_REQ_SET_ENCODE)        = __wioctl_set_encode,
	REQ_NUM(ACFG_REQ_GET_ENCODE)        = __wioctl_get_encode,
	REQ_NUM(ACFG_REQ_SET_VAP_PARAM)     = __wioctl_set_vap_param,
	REQ_NUM(ACFG_REQ_GET_VAP_PARAM)     = __wioctl_get_vap_param,
	REQ_NUM(ACFG_REQ_SET_VAP_VENDOR_PARAM)     = __wioctl_set_vap_vendor_param,
	REQ_NUM(ACFG_REQ_GET_VAP_VENDOR_PARAM)     = __wioctl_get_vap_vendor_param,
	REQ_NUM(ACFG_REQ_SET_VAP_WMMPARAMS) = __wioctl_set_vap_wmmparams,
	REQ_NUM(ACFG_REQ_GET_VAP_WMMPARAMS) = __wioctl_get_vap_wmmparams,
	REQ_NUM(ACFG_REQ_SET_RATE)          = __wioctl_set_rate,
	REQ_NUM(ACFG_REQ_GET_RATE)          = __wioctl_get_rate,
	REQ_NUM(ACFG_REQ_SET_SCAN)          = __wioctl_set_scan,
	REQ_NUM(ACFG_REQ_GET_SCANRESULTS)   = __wioctl_get_scan_results,
	REQ_NUM(ACFG_REQ_GET_STATS)         = __wioctl_get_stats,
	REQ_NUM(ACFG_REQ_SET_PHYMODE)       = __wioctl_set_phymode,
	REQ_NUM(ACFG_REQ_GET_PHYMODE)       = __wioctl_get_phymode,
	REQ_NUM(ACFG_REQ_GET_ASSOC_STA_INFO)= __wioctl_get_assoc_sta_info,
	REQ_NUM(ACFG_REQ_SET_POWMGMT)       = __wioctl_set_powmgmt,
	REQ_NUM(ACFG_REQ_GET_POWMGMT)       = __wioctl_get_powmgmt,
	REQ_NUM(ACFG_REQ_SET_CHMODE)        = __wioctl_set_chmode,
	REQ_NUM(ACFG_REQ_GET_CHMODE)        = __wioctl_get_chmode,
	REQ_NUM(ACFG_REQ_GET_RSSI)          = __wioctl_get_rssi,
	REQ_NUM(ACFG_REQ_GET_TESTMODE)      = __wioctl_get_testmode,
	REQ_NUM(ACFG_REQ_SET_TESTMODE)      = __wioctl_set_testmode,
	REQ_NUM(ACFG_REQ_NAWDS_CONFIG)      = __wioctl_config_nawds,
	REQ_NUM(ACFG_REQ_GET_CUSTDATA)      = __wioctl_get_custdata,
	REQ_NUM(ACFG_REQ_WSUPP_INIT)        = __wioctl_wsupp_init,
	REQ_NUM(ACFG_REQ_WSUPP_FINI)        = __wioctl_wsupp_fini,
	REQ_NUM(ACFG_REQ_WSUPP_IF_ADD)      = __wioctl_wsupp_if_add,
	REQ_NUM(ACFG_REQ_WSUPP_IF_RMV)      = __wioctl_wsupp_if_remove,
	REQ_NUM(ACFG_REQ_WSUPP_NW_CRT)      = __wioctl_wsupp_nw_create,
	REQ_NUM(ACFG_REQ_WSUPP_NW_DEL)      = __wioctl_wsupp_nw_delete,
	REQ_NUM(ACFG_REQ_WSUPP_NW_SET)      = __wioctl_wsupp_nw_set,
	REQ_NUM(ACFG_REQ_WSUPP_NW_GET)      = __wioctl_wsupp_nw_get,
	REQ_NUM(ACFG_REQ_WSUPP_NW_LIST)     = __wioctl_wsupp_nw_list,
	REQ_NUM(ACFG_REQ_WSUPP_WPS_REQ)     = __wioctl_wsupp_wps_req,
	REQ_NUM(ACFG_REQ_WSUPP_SET)         = __wioctl_wsupp_set,
	REQ_NUM(ACFG_REQ_DOTH_CHSWITCH)     = __wioctl_doth_chswitch,
	REQ_NUM(ACFG_REQ_ADDMAC)            = __wioctl_addmac,
	REQ_NUM(ACFG_REQ_DELMAC)            = __wioctl_delmac,
	REQ_NUM(ACFG_REQ_KICKMAC)           = __wioctl_kickmac,
	REQ_NUM(ACFG_REQ_SET_MLME)          = __wioctl_set_mlme,
	REQ_NUM(ACFG_REQ_SEND_MGMT)         = __wioctl_send_mgmt,
	REQ_NUM(ACFG_REQ_SET_OPTIE)         = __wioctl_set_optie,
	REQ_NUM(ACFG_REQ_SET_ACPARAMS)      = __wioctl_set_acparams,
	REQ_NUM(ACFG_REQ_SET_FILTERFRAME)   = __wioctl_set_filterframe,
	REQ_NUM(ACFG_REQ_SET_APPIEBUF)      = __wioctl_set_appiebuf,
	REQ_NUM(ACFG_REQ_SET_KEY)           = __wioctl_set_key,
	REQ_NUM(ACFG_REQ_DEL_KEY)           = __wioctl_del_key,
	REQ_NUM(ACFG_REQ_GET_CHAN_INFO)     = __wioctl_get_chan_info,
	REQ_NUM(ACFG_REQ_GET_CHAN_LIST)     = __wioctl_get_chan_list,
	REQ_NUM(ACFG_REQ_GET_MAC_ADDR)      = __wioctl_get_mac_address,
	REQ_NUM(ACFG_REQ_GET_VAP_P2P_PARAM) = __wioctl_get_vap_p2p_param,
	REQ_NUM(ACFG_REQ_SET_VAP_P2P_PARAM) = __wioctl_set_vap_p2p_param,
	REQ_NUM(ACFG_REQ_DBGREQ)            = __wioctl_dbgreq,
	REQ_NUM(ACFG_REQ_ACL_ADDMAC)	= __wioctl_acl_addmac,
	REQ_NUM(ACFG_REQ_ACL_GETMAC)	= __wioctl_acl_getmac,
	REQ_NUM(ACFG_REQ_ACL_DELMAC)	= __wioctl_acl_delmac,
	REQ_NUM(ACFG_REQ_IS_OFFLOAD_VAP)	= __wioctl_is_offload_vap,
	REQ_NUM(ACFG_REQ_SET_CHNWIDTHSWITCH)= __wioctl_set_chn_widthswitch,
	REQ_NUM(ACFG_REQ_SET_ATF_ADDSSID) =  __wioctl_set_atf_addssid,
	REQ_NUM(ACFG_REQ_SET_ATF_DELSSID) =  __wioctl_set_atf_delssid,
	REQ_NUM(ACFG_REQ_SET_ATF_ADDSTA) =  __wioctl_set_atf_addsta,
	REQ_NUM(ACFG_REQ_SET_ATF_DELSTA) =  __wioctl_set_atf_delsta,
//	REQ_NUM(ACFG_REQ_MON_ADDMAC)    = __wioctl_mon_addmac,
//	REQ_NUM(ACFG_REQ_MON_DELMAC)    = __wioctl_mon_delmac,
	/* <add here> */
	REQ_NUM(ACFG_REQ_LAST_VAP)          = 0, /* Last VAP entry */
};

/**
 * @brief Create the VAP
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_create_vap(__qdf_softc_t    *sc, acfg_data_t  *data)
{
	uint32_t status = QDF_STATUS_E_INVAL;
	acfg_vapinfo_t  *vap_info = (acfg_vapinfo_t *)data;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);
	if(!sc2wifi_cfg(sc)->create_vap)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2wifi_cfg(sc)->create_vap(sc->drv_hdl, vap_info->icp_name,
										vap_info->icp_opmode,
										vap_info->icp_vapid,
										vap_info->icp_flags);

	return status;

}

/**
 * @brief Set the SSID
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_ssid(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_ssid)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_ssid(sc->drv_hdl, &data->ssid);

	return status;
}

/**
 * @brief Get the SSID
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_ssid(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_ssid)
	return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_ssid(sc->drv_hdl, &data->ssid);

	return status;
}

/**
 * @brief Set the TESTMODE
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_testmode(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_testmode)
	return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_testmode(sc->drv_hdl, &data->testmode);

	return status;
}

/**
 * @brief Get the TESTMODE
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_testmode(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_testmode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_testmode(sc->drv_hdl, &data->testmode);

	return status;
}

/**
 * @brief Get the RSSI
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_rssi(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_rssi)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_rssi(sc->drv_hdl, &data->rssi);

	return status;
}

/**
 * @brief Get the CUSTDATA
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_custdata(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_custdata)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_custdata(sc->drv_hdl, &data->custdata);

	return status;
}


/**
 * @brief Set the channel (IEEE)
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_channel(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Set channel to %d \n",
					__FUNCTION__, data->chan);

	if(!sc2vap_cfg(sc)->set_channel)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_channel(sc->drv_hdl, data->chan);

	return status;
}


/**
 * @brief Get the channel (IEEE)
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_channel(__qdf_softc_t    *sc, acfg_data_t    *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_channel)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_channel(sc->drv_hdl, &data->chan);

	return status;
}


/**
 * @brief Delete VAP
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_delete_vap(__qdf_softc_t    *sc, acfg_data_t    *data)
{
	uint32_t status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Delete vap - %s \n",\
				__FUNCTION__, data->vap_info.icp_name);

	if(!sc2vap_cfg(sc)->delete_vap)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->delete_vap(sc->drv_hdl);

	return status;
}



/**
 * @brief Is the VAP offload (remote) or local.
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_is_offload_vap(__qdf_softc_t    *sc, acfg_data_t    *data)
{
	uint32_t status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Is offload vap - %s \n",\
				__FUNCTION__, data->vap_info.icp_name);

	/* This API was invoked on a VAP handled by the
	 * offload stack. We return true here.
	*/
	status = QDF_STATUS_SUCCESS ;

	return status;
}


 /**
 * @brief Set Opmode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_opmode(__qdf_softc_t    *sc, acfg_data_t    *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Set Opmode to %d \n",\
										__FUNCTION__, data->opmode);

	if(!sc2vap_cfg(sc)->set_opmode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_opmode(sc->drv_hdl, data->opmode);

	return status ;
}

/**
 * @brief Get Opmode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_opmode(__qdf_softc_t    *sc, acfg_data_t    *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_opmode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_opmode(sc->drv_hdl, &data->opmode);

	return status ;
}


/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_freq(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): mantissa = %d ; exponent = %d\n",\
			__FUNCTION__,data->freq.m,data->freq.e);

	if(!sc2vap_cfg(sc)->set_freq)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_freq(sc->drv_hdl, &data->freq);

	return status;
}


/**
 * @brief Get the frequency
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_freq(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_freq)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_freq(sc->drv_hdl, &data->freq);

	return status;
}


/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_wireless_name(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_wireless_name)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_wireless_name(sc->drv_hdl, \
								(u_int8_t *)data, ACFG_MAX_IFNAME);

	return status;
}



/**
 * @brief Set RTS threshold
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_rts(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t      status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_rts)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_rts(sc->drv_hdl, &data->rts);

	return status;
}



/**
 * @brief Get RTS threshold
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_rts(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_rts)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_rts(sc->drv_hdl, &data->rts);

	return status;
}



/**
 * @brief Set Fragmentation threshold
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_frag(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_frag)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_frag(sc->drv_hdl, &data->frag);

	return status;
}



/**
 * @brief Get Fragmentation threshold
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_frag(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_frag)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_frag(sc->drv_hdl, &data->frag);

	return status;
}


/**
 * @brief Get default Tx Power (dBm)
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_txpow(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_txpow)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_txpow(sc->drv_hdl, &data->txpow);

	return status;
}


/**
 * @brief Set default Tx Power (dBm)
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_txpow(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_txpow)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_txpow(sc->drv_hdl, &data->txpow);

	return status;
}


/**
 * @brief Set Access Point MAC Address
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_ap(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_ap)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_ap(sc->drv_hdl, &data->macaddr);

	return status;
}


/**
 * @brief Get Access Point MAC Address
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_ap(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_ap)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_ap(sc->drv_hdl, &data->macaddr);

	return status;
}


/**
 * @brief Get Range of Parameters
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_range(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_range)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_range(sc->drv_hdl, &data->range);

	return status;
}



/**
 * @brief Set encoding token and mode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_encode(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_encode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_encode(sc->drv_hdl, &data->encode);

	return status;
}



/**
 * @brief Get encoding token and mode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_encode(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_encode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_encode(sc->drv_hdl, &data->encode);

	return status;
}



/**
 * @brief Set a VAP param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_vap_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_vap_param)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_vap_param(sc->drv_hdl, \
			data->param_req.param, data->param_req.val);

	return status;
}



/**
 * @brief Get a VAP param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_vap_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_vap_param)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_vap_param(sc->drv_hdl, \
			data->param_req.param, &data->param_req.val);

	return status;
}

/**
 * @brief Set a VAP vendor param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_vap_vendor_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_vap_vendor_param)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_vap_vendor_param(sc->drv_hdl, \
			&data->vendor_param_req);

	return status;
}

/**
 * @brief Get a VAP vendor param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_vap_vendor_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_vap_vendor_param)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_vap_vendor_param(sc->drv_hdl, \
			&data->vendor_param_req);

	return status;
}

/**
 * @brief Set a VAP wmm param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_vap_wmmparams(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_wmmparams)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_wmmparams(sc->drv_hdl, \
			data->wmmparams_req.param, data->wmmparams_req.val);

	return status;
}

/**
 * @brief Get a VAP wmm params
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_vap_wmmparams(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_wmmparams)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_wmmparams(sc->drv_hdl, \
			data->wmmparams_req.param, &data->wmmparams_req.val);

	return status;
}

/**
 * @brief Set a Radio param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_radio_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->set_radio_param)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2wifi_cfg(sc)->set_radio_param(sc->drv_hdl,
											  data->param_req.param,
											  data->param_req.val);

	return status;
}



/**
 * @brief Get a Radio param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_radio_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->get_radio_param)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2wifi_cfg(sc)->get_radio_param(sc->drv_hdl,
											  data->param_req.param,
											  &data->param_req.val);

	return status;
}



/**
 * @brief Set Channel mode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_chmode(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_chmode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_chmode(sc->drv_hdl, (acfg_chmode_t *)data);

	return status;
}


/**
 * @brief Get Channel mode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_chmode(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_chmode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_chmode(sc->drv_hdl, (acfg_chmode_t *)data);

	return status;
}


/**
 * @brief Set default bit rate
 *
 * @param sc
 * @param data
 *
 * @return
*/
static uint32_t
__wioctl_set_rate(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, " %s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_rate)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_rate(sc->drv_hdl, (acfg_rate_t *)data);

	return status;
}


/**
 * @brief Get default bit rate
 *
 * @param sc
 * @param data
 *
 * @return
*/
static uint32_t
__wioctl_get_rate(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_rate)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_rate(sc->drv_hdl, &data->bitrate);

	return status;
}

 /**
  * @brief Get power management
  *
  * @param sc
  * @param data
  *
  * @return
  */
 static uint32_t
 __wioctl_set_powmgmt(__qdf_softc_t *sc, acfg_data_t *data)
 {
	 uint32_t 	  status = QDF_STATUS_E_INVAL;

	 qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	 if(!sc2vap_cfg(sc)->set_powmgmt)
		 return QDF_STATUS_E_NOSUPPORT ;

	 status = sc2vap_cfg(sc)->set_powmgmt(sc->drv_hdl, &data->powmgmt);

	 return status;
 }

 /**
  * @brief Get power management
  *
  * @param sc
  * @param data
  *
  * @return
  */
 static uint32_t
 __wioctl_get_powmgmt(__qdf_softc_t *sc, acfg_data_t *data)
 {
	 uint32_t 	  status = QDF_STATUS_E_INVAL;

	 qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	 if(!sc2vap_cfg(sc)->get_powmgmt)
		 return QDF_STATUS_E_NOSUPPORT ;

	 status = sc2vap_cfg(sc)->get_powmgmt(sc->drv_hdl, &data->powmgmt);

	 return status;
 }


 /**
  * @brief Set Reg value
  *
  * @param sc
  * @param data
  *
  * @return
  */
static uint32_t
__wioctl_set_reg(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t 	  status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->set_reg)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2wifi_cfg(sc)->set_reg(sc->drv_hdl, data->param_req.param,
									  data->param_req.val);

	return status;
}


 /**
  * @brief Get a Reg value
  *
  * @param sc
  * @param data
  *
  * @return
  */
static uint32_t
__wioctl_get_reg(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t 	  status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->get_reg)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2wifi_cfg(sc)->get_reg(sc->drv_hdl, data->param_req.param,
									  &data->param_req.val);

	return status;
}


  /**
   * @brief Set hw address
   *
   * @param sc
   * @param data
   *
   * @return
   */
 uint32_t
 __wioctl_set_hwaddr(__qdf_softc_t *sc, acfg_data_t *data)
 {
	 uint32_t    status = QDF_STATUS_E_INVAL;

	 qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	 if(!sc2wifi_cfg(sc)->set_hwaddr)
		 return QDF_STATUS_E_NOSUPPORT;

	 status = sc2wifi_cfg(sc)->set_hwaddr(sc->drv_hdl, &data->macaddr);

	 return status;
 }

 /* @brief Set the Scan Request
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_scan(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;


	if(!sc2vap_cfg(sc)->set_scan)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_scan(sc->drv_hdl, &data->sscan);

	return status;
}

/**
 * @brief Get ath_stats
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_ath_stats(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->get_ath_stats)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2wifi_cfg(sc)->get_ath_stats(sc->drv_hdl, &data->ath_stats);

	return status;
}

/**
 * @brief clear ath_stats
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_clr_ath_stats(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->clr_ath_stats)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2wifi_cfg(sc)->clr_ath_stats(sc->drv_hdl, NULL);

	return status;
}

/**
 * @brief Get the Scan Result
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_scan_results(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;


	if(!sc2vap_cfg(sc)->get_scan_results)
		return QDF_STATUS_E_NOSUPPORT ;
	status = sc2vap_cfg(sc)->get_scan_results(sc->drv_hdl, data->scan);

	return status;
}


/**
 * @brief Get wireless statistics
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_stats(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_stats)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->get_stats(sc->drv_hdl, &data->stats);

	return status;
}


/**
 * @brief Set Phymode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_phymode(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): phymode - %d\n",\
									 __FUNCTION__,data->phymode);

	if(!sc2vap_cfg(sc)->set_phymode)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_phymode(sc->drv_hdl, data->phymode);

	return status;
}

/**
 * @brief Get Phymode
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_phymode(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",\
									 __FUNCTION__);

	if(!sc2vap_cfg(sc)->get_phymode)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->get_phymode(sc->drv_hdl, &data->phymode);

	return status;
}

/*
 * Convert MHz frequency to IEEE channel number.
 */
uint32_t
__qdf_mhz2ieee(uint32_t freq)
{
#define IS_CHAN_IN_PUBLIC_SAFETY_BAND(_c) ((_c) > 4940 && (_c) < 4990)
	if (freq == 2484)
		return 14;

	if (freq < 2484)
		return (freq - 2407) / 5;

	if (freq < 5000) {
		if (IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
			return ((freq * 10) + (((freq % 5) == 2) ? 5 : 0) - 49400)/5;
		} else if (freq > 4900) {
			return (freq - 4000) / 5;
		} else {
			return 15 + ((freq - 2512) / 20);
		}
	}
	return (freq - 5000) / 5;
}


/**
 * @brief Set MLME
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_mlme(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_mlme)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_mlme(sc->drv_hdl, data->mlme);

	return status;
}


/**
 * @brief Send Mgmt
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_send_mgmt(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->send_mgmt)
		return QDF_STATUS_E_NOSUPPORT ;

		status = sc2vap_cfg(sc)->send_mgmt(sc->drv_hdl, &data->mgmt);

	return status;
}



/**
 * @brief Set Optional IE
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_optie(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_optie)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_optie(sc->drv_hdl, &data->ie);

	return status;
}


/**
 * @brief Set acparams
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_acparams(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_acparams)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_acparams(sc->drv_hdl, data->acparams);

	return status;
}



/**
 * @brief Set filterframe
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_filterframe(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_filterframe)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_filterframe(sc->drv_hdl, &data->filter);

	return status;
}

/**
  * @brief Send dbgreq frame
  *
  * @param sc
  * @param data
  *
  * @return
  */
static uint32_t
__wioctl_dbgreq(__qdf_softc_t *sc, acfg_data_t *data)
{
   uint32_t       status = QDF_STATUS_E_INVAL;

   qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

   if(!sc2vap_cfg(sc)->dbgreq)
	   return QDF_STATUS_E_NOSUPPORT ;

   status = sc2vap_cfg(sc)->dbgreq(sc->drv_hdl, &data->dbgreq);

   return status;
}

/**
 * @brief Set Appiebuf
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_appiebuf(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_appiebuf)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_appiebuf(sc->drv_hdl, &data->appie);

	return status;
}


/**
 * @brief Set SET KEY
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_key(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_key)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_key(sc->drv_hdl, &data->key);

	return status;
}

/**
 * @brief Del Key
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_del_key(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->del_key)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->del_key(sc->drv_hdl, &data->delkey);

	return status;
}


/**
 * @brief Get Channel List
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_chan_list(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_chan_list)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->get_chan_list(sc->drv_hdl, &data->opq);

	return status;
}


/**
 * @brief Channel Info
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_chan_info(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_chan_info)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->get_chan_info(sc->drv_hdl, &data->chan_info);

	return status;
}


/**
 * @brief Get ACL Mac Address List
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_mac_address(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->get_mac_address)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->get_mac_address(sc->drv_hdl, &data->maclist);

	return status;
}


/**
 * @brief Get P2P Big Param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_vap_p2p_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);

	if(!sc2vap_cfg(sc)->get_p2p_param)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->get_p2p_param(sc->drv_hdl, &data->p2p_param);

	return status;
}


/**
 * @brief Set P2P Big Param
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_set_vap_p2p_param(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);

	if(!sc2vap_cfg(sc)->set_p2p_param)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->set_p2p_param(sc->drv_hdl, &data->p2p_param);

	return status;
}

static uint32_t
__wioctl_mon_delmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_SUCCESS;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);

	if(!sc2vap_cfg(sc)->mon_delmac)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->mon_delmac(sc->drv_hdl, &data->macaddr, 1);

	return status;
}

static uint32_t
__wioctl_mon_addmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_SUCCESS;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);

	if(!sc2vap_cfg(sc)->mon_addmac)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->mon_addmac(sc->drv_hdl, &data->macaddr, 1);

	return status;
}

static uint32_t
__wioctl_acl_addmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_SUCCESS;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);

	if(!sc2vap_cfg(sc)->acl_setmac)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->acl_setmac(sc->drv_hdl, &data->macaddr, 1);

	return status;
}

static uint32_t
__wioctl_acl_getmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_SUCCESS;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);


	return status;
}

static uint32_t
__wioctl_phyerr(__qdf_softc_t *sc, acfg_data_t *data)
{

	uint32_t status = QDF_STATUS_SUCCESS;

	if(!sc2wifi_cfg(sc)->phyerr)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2wifi_cfg(sc)->phyerr(sc->drv_hdl, &data->ath_diag);

	return status;
}

static uint32_t
__wioctl_acl_delmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_SUCCESS;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);

	if(!sc2vap_cfg(sc)->acl_setmac)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->acl_setmac(sc->drv_hdl, &data->macaddr, 0);

	return status;
}

static uint32_t
__wioctl_set_chn_widthswitch(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;
	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_chn_widthswitch)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->set_chn_widthswitch(sc->drv_hdl, &data->chnw);

	return status;
}

static uint32_t
__wioctl_set_atf_addssid(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;
	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_atf_ssid)
		return QDF_STATUS_E_NOSUPPORT;

	data->atf_ssid.id_type = ACFG_ATF_ADDSSID;

	status = sc2vap_cfg(sc)->set_atf_ssid(sc->drv_hdl, &data->atf_ssid);

	return status;
}

static uint32_t
__wioctl_set_atf_delssid(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;
	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_atf_ssid)
		return QDF_STATUS_E_NOSUPPORT;

	data->atf_ssid.id_type = ACFG_ATF_DELSSID;

	status = sc2vap_cfg(sc)->set_atf_ssid(sc->drv_hdl, &data->atf_ssid);

	return status;
}

static uint32_t
__wioctl_set_atf_addsta(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;
	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_atf_sta)
		return QDF_STATUS_E_NOSUPPORT;

	data->atf_sta.id_type = ACFG_ATF_ADDSTA;

	status = sc2vap_cfg(sc)->set_atf_sta(sc->drv_hdl, &data->atf_sta);

	return status;
}

static uint32_t
__wioctl_set_atf_delsta(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;
	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->set_atf_sta)
		return QDF_STATUS_E_NOSUPPORT;

	data->atf_sta.id_type = ACFG_ATF_DELSTA;

	status = sc2vap_cfg(sc)->set_atf_sta(sc->drv_hdl, &data->atf_sta);

	return status;
}

static uint32_t
__wioctl_set_country(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;
	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->set_country)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2wifi_cfg(sc)->set_country(sc->drv_hdl, &data->setcntry);

	return status;
}

static uint32_t
__wioctl_get_wlan_profile(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_SUCCESS;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n", __FUNCTION__);

	if(!sc2wifi_cfg(sc)->get_profile)
		return QDF_STATUS_E_NOSUPPORT;
	status = sc2wifi_cfg(sc)->get_profile(sc->drv_hdl, &data->radio_vap_info);

	return status;
}
/**
 * @brief Get info on associated stations
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_get_assoc_sta_info(__qdf_softc_t *sc, acfg_data_t *data)
{
	void *buff = NULL ;
	uint32_t status = QDF_STATUS_E_FAILURE;
	uint32_t retlen = 0, ret;
	uint8_t *cp , *ucp;
	acfg_sta_info_t *pinfo = NULL ;

	if(!sc2vap_cfg(sc)->get_sta_info)
		return QDF_STATUS_E_NOSUPPORT ;

	buff = vmalloc(WCFG_STA_INFO_SPACE + sizeof(uint32_t)) ;
	if(!buff)
		return QDF_STATUS_E_NOMEM ;

	pinfo = vmalloc(data->sta_info.len) ;
	if(!pinfo)
	{
		vfree(buff) ;
		return QDF_STATUS_E_NOMEM ;
	}
	memset(pinfo, 0, data->sta_info.len);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Calling atd for "\
					"associated station info. \n",__FUNCTION__);

	status = sc2vap_cfg(sc)->get_sta_info(sc->drv_hdl, \
									buff, WCFG_STA_INFO_SPACE + sizeof(uint32_t));
	if(status != QDF_STATUS_SUCCESS)
		goto fail;

	retlen = *(uint32_t *)buff;
	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Returned len = %d\n",__FUNCTION__, \
			  retlen);

	/*
	 * Copy data from 'buff' to user buffer
	 */
	ucp = (uint8_t *)pinfo;
	cp = (uint8_t *)buff + sizeof(uint32_t) ;
	do
	{
		uint32_t i ;
		struct wcfg_sta_info *si = (struct wcfg_sta_info *)cp ;
		acfg_sta_info_t *usi = (acfg_sta_info_t *)ucp ;

		usi->associd = (si->isi_associd &~ 0xc000) ;
		usi->channel = __qdf_mhz2ieee(si->isi_freq) ;

		if(si->isi_txratekbps == 0)
			usi->txrate = (si->isi_rates[si->isi_txrate] & \
											WCFG_RATE_VAL)/2;
		else
			usi->txrate = si->isi_txratekbps / 1000;

		usi->rssi = si->isi_rssi ;
		usi->inact = si->isi_inact ;
//		usi->rxseq = si->isi_rxseqs;
        memcpy(usi->rxseq, si->isi_rxseqs, sizeof(usi->rxseq));
        memcpy(usi->txseq, si->isi_txseqs, sizeof(usi->txseq));
//		usi->txseq = si->isi_txseqs;
		usi->cap = si->isi_capinfo ;
		usi->athcap = si->isi_athflags ;
		usi->htcap = si->isi_htcap ;
		usi->state = si->isi_state ;
		usi->erp = si->isi_erp ;

		for(i = 0 ; i < ACFG_MACADDR_LEN; i++)
			usi->macaddr[i] = si->isi_macaddr[i] ;

		qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Associd - %d; ",__FUNCTION__,\
				  usi->associd);

		ucp += sizeof(*usi) ;
		cp += si->isi_len;
		retlen -= si->isi_len ;
	}while(retlen >= sizeof(struct wcfg_sta_info) && \
			ucp < (uint8_t *)pinfo + data->sta_info.len);

	/*
	 * Update request structure with amount
	 * of data actually in the buffer.
	 */
	data->sta_info.len = ucp - (uint8_t *)pinfo ;

	ret = copy_to_user(data->sta_info.info, (void *)pinfo, data->sta_info.len);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Copied %d to user space",\
									__FUNCTION__, data->sta_info.len);

fail:
	vfree(buff) ;
	vfree(pinfo) ;
	return status;
}


/**
 * @brief tx99tool
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_tx99tool(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2wifi_cfg(sc)->tx99tool)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2wifi_cfg(sc)->tx99tool(sc->drv_hdl, &data->tx99_wcmd);

	return status;
}


/**
 * @brief config_nawds
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_config_nawds(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->config_nawds)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->config_nawds(sc->drv_hdl, &data->nawds_cmd);

	return status;
}



/**
 * @brief Init security service
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_init(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_init)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->wsupp_init(sc->drv_hdl, &data->wsupp_info);

	return status;
}

/**
 * @brief Fini security service
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_fini(__qdf_softc_t  *sc, acfg_data_t  *data)
{
	uint32_t   status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_fini)
		return QDF_STATUS_E_NOSUPPORT ;

	status = sc2vap_cfg(sc)->wsupp_fini(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge add interface
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_if_add(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_if_add)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_if_add(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge remove interface
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_if_remove(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_if_remove)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_if_remove(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge create network
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_nw_create(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_nw_create)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_nw_create(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge delete network
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_nw_delete(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_nw_delete)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_nw_delete(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge network setting
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_nw_set(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_nw_set)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_nw_set(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge network get attribute value
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_nw_get(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_nw_get)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_nw_get(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge network list
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_nw_list(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_nw_list)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_nw_list(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant bridge WPS request
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_wps_req(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_wps_req)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_wps_req(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Supplicant setting at run time
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_wsupp_set(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->wsupp_set)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->wsupp_set(sc->drv_hdl, &data->wsupp_info);

	return status;
}


/**
 * @brief Switch the channel
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_doth_chswitch(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->doth_chsw)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->doth_chsw(sc->drv_hdl, &data->doth_chsw);

	return status;
}


/**
 * @brief Add mac address to ACL list
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_addmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->addmac)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->addmac(sc->drv_hdl, &data->macaddr);

	return status;
}



/**
 * @brief Delete MAC address from ACL list
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_delmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->delmac)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->delmac(sc->drv_hdl, &data->macaddr);

	return status;
}


/**
 * @brief
 *
 * @param sc
 * @param data
 *
 * @return
 */
static uint32_t
__wioctl_kickmac(__qdf_softc_t *sc, acfg_data_t *data)
{
	uint32_t       status = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): \n",__FUNCTION__);

	if(!sc2vap_cfg(sc)->kickmac)
		return QDF_STATUS_E_NOSUPPORT;

	status = sc2vap_cfg(sc)->kickmac(sc->drv_hdl, &data->macaddr);

	return status;
}



/**
 * @brief Generic IOCTL parser
 *
 * @param netdev
 * @param data
 *
 * @return
 */
int
__qdf_net_wifi_wioctl(struct net_device     *netdev, struct ifreq   *ifr)
{
	__qdf_softc_t   *sc = netdev_to_softc(netdev);
	acfg_os_req_t   *req = NULL;
	uint32_t      req_len = sizeof(struct acfg_os_req);
	uint32_t      status  = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): ifname %s\n",
			  __FUNCTION__, netdev->name);

	req = vmalloc(req_len);
	if (!req) return -ENOMEM;

	memset(req, 0, req_len);

	/* XXX: Optimize only copy the amount thats valid */
	if (copy_from_user(req, ifr->ifr_data, req_len))
		goto done;

	if ((req->cmd <= ACFG_REQ_FIRST_WIFI) ||
		(req->cmd >= ACFG_REQ_LAST_WIFI) ||
		!__wioctl_sw[req->cmd])
		goto done;

	status = __wioctl_sw[req->cmd](sc, &req->data);

	/* XXX: Optimize only copy the amount thats valid */
	if (copy_to_user(ifr->ifr_data, req, req_len))
		status = QDF_STATUS_E_INVAL;

done:
	vfree(req);

	return __qdf_status_to_os(status);
}

/**
 * @brief Generic IOCTL parser
 *
 * @param netdev
 * @param data
 *
 * @return
 */
int
__qdf_net_vap_wioctl(struct net_device     *netdev, struct ifreq   *ifr)
{
	__qdf_softc_t        *sc = netdev_to_softc(netdev);
	acfg_os_req_t   *req    = NULL;
	uint32_t      req_len = sizeof(struct acfg_os_req);
	uint32_t      status  = QDF_STATUS_E_INVAL;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): ifname %s\n",
			  __FUNCTION__, netdev->name);

	req = vmalloc(req_len);
	if (!req) return -ENOMEM;

	memset(req, 0, req_len);

	/* XXX: Optimize only copy the amount thats valid */
	if (copy_from_user(req, ifr->ifr_data, req_len))
		goto done;

	if ((req->cmd <= ACFG_REQ_FIRST_VAP) ||
		(req->cmd >= ACFG_REQ_LAST_VAP) ||
		!__wioctl_sw[req->cmd])
		goto done;


	status = __wioctl_sw[req->cmd](sc, &req->data);

	/* XXX: Optimize only copy the amount thats valid */
	if (copy_to_user(ifr->ifr_data, req, req_len))
		status = QDF_STATUS_E_INVAL;

done:
	vfree(req);

	return __qdf_status_to_os(status);
}
/* ****************************************************
 * ****************************************************
 *      PRIVATE IOCTL handlers (SIOCDEVPRIVATE)
 * ****************************************************
 * ****************************************************
 */


/*
 * Copied from ieee80211_ioctl.h (wlanconfig).
*/
#define WCFG_ADDR_LEN       6
struct wcfg_clone_params {
	char icp_name[ACFG_MAX_IFNAME];     /* device name */
	u_int16_t   icp_opmode;             /* operating mode */
	u_int16_t   icp_flags;              /* see below */
	u_int8_t icp_bssid[WCFG_ADDR_LEN];  /* for WDS links */
	int32_t   icp_vapid;                 /* vap id for MAC addr req */
};
#define WCFG_CLONE_BSSID       0x0001   /* allocate unique mac/bssid */
#define WCFG_NO_STABEACONS     0x0002   /* Do not setup the station
										   beacon timers */
#define WCFG_CLONE_WDS         0x0004   /* enable WDS processing */
#define WCFG_CLONE_WDSLEGACY   0x0008   /* legacy WDS operation */



/**
 * @brief Get athstat results
 *
 * @param netdev
 * @param iwr
 *
 * @return
 */
static int
__qdf_net_wifi_getath_stats(struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t        *sc = netdev_to_softc(netdev);
	acfg_ath_stats_t       *user    = NULL,
							local   = {0};
	uint32_t              error   = QDF_STATUS_E_NOMEM;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): athstats tool \n", __FUNCTION__);

	user = (acfg_ath_stats_t *)ifr->ifr_data;
	if (user->size <= 0 || !user->address) {
		printk("%s : Invalid len or addr\n",__FUNCTION__);
		error = QDF_STATUS_E_INVAL;
		goto done;
	}

	local.size        = user->size;
	local.address     = vmalloc(local.size);
	local.offload_if  = user->offload_if;
	if (!local.address) goto done;

	memset(local.address, 0, local.size);

	error = __wioctl_sw[ACFG_REQ_GET_ATHSTATS](sc, (acfg_data_t *)&local);

	user->size = local.size;
	user->offload_if = local.offload_if;

	if (copy_to_user(user->address, local.address, local.size))
		error = QDF_STATUS_E_NOMEM;

	vfree(local.address);
done:
	return __qdf_status_to_os(error);
}

/**
 * @brief clear athstats
 *
 * @param netdev
 * @param iwr
 *
 * @return
 */
static int
__qdf_net_wifi_clrath_stats(struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t          *sc      = netdev_to_softc(netdev);
	uint32_t              error   = QDF_STATUS_E_FAILURE;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): ath_stats clear \n", __FUNCTION__);

	error = __wioctl_sw[ACFG_REQ_CLR_ATHSTATS](sc, NULL);

	return __qdf_status_to_os(error);
}

#define vzalloc(size, ptr) \
	do {\
		ptr = vmalloc(size);\
		if(ptr)\
		memset(ptr, 0, size);\
	}while(0);

static int
__qdf_net_wifi_phyerr( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t      *sc      = netdev_to_softc(netdev);
	acfg_ath_diag_t     user = {{0}};
	uint32_t          error   = QDF_STATUS_E_NOMEM;
	void               *local_indata  = NULL;
	void               *local_outdata = NULL;
	void               *user_outdata = NULL;

	if (copy_from_user(&user, ifr->ifr_data, sizeof(acfg_ath_diag_t)))
		goto done;

	if (user.ad_id & 0x4000) {
		local_indata = qdf_mem_malloc(user.ad_in_size);
		if (local_indata == NULL)
			goto done;

		/* Currently we support only bangradar, enable, disable and ignorecac.
		 * Out of these only ignorecac has a in-param, of size 4 bytes. */
		if (copy_from_user(local_indata, user.ad_in_data, user.ad_in_size))
			goto done;

		user.ad_in_data = local_indata;
	}
	if(user.ad_out_size && user.ad_out_data)
	{
		vzalloc(user.ad_out_size, local_outdata);
		if (local_outdata == NULL)
			goto done;
		user_outdata = user.ad_out_data;
		user.ad_out_data = local_outdata;
	}

	error = __wioctl_sw[ACFG_REQ_PHYERR](sc, (acfg_data_t *)&user);

	if(user.ad_out_size && user.ad_out_data)
	{
		if(copy_to_user(user_outdata, local_outdata, user.ad_out_size))
			error = QDF_STATUS_E_NOMEM;
	}

done:
	if (local_indata)
		qdf_mem_free(local_indata);
	if (local_outdata)
		vfree(local_outdata);

	return __qdf_status_to_os(error);
}

static int
__qdf_net_wifi_create_vap( struct net_device *netdev, struct ifreq *ifr)
{
	struct wcfg_clone_params    user    = {{0}};
	acfg_vapinfo_t              local   = {0};
	__qdf_softc_t              *sc      = netdev_to_softc(netdev);
	uint32_t                  error   = QDF_STATUS_E_INVAL;

	if(copy_from_user(&user, ifr->ifr_data, sizeof(struct wcfg_clone_params)))
		goto done ;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Vap Create - %s on %s \n",
			  __FUNCTION__, user.icp_name, netdev->name);

	local.icp_opmode    = user.icp_opmode ;
	local.icp_flags     = user.icp_flags ;
	memcpy(local.icp_name, user.icp_name, ACFG_MAX_IFNAME);

	error = __wioctl_sw[ACFG_REQ_CREATE_VAP](sc, (acfg_data_t *)&local);

	memcpy(ifr->ifr_ifrn.ifrn_name, local.icp_name, ACFG_MAX_IFNAME);

done:
	return __qdf_status_to_os(error) ;
}

static int
__qdf_net_wifi_tx99tool(struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t      *sc      = netdev_to_softc(netdev);
	acfg_tx99_t         local   = {{0}};
	uint32_t          error   = QDF_STATUS_E_NOMEM;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): tx99tool IOCTL \n", __FUNCTION__);

	if(copy_from_user(&local, ifr->ifr_data, sizeof(struct acfg_tx99)))
		goto done;

	error = __wioctl_sw[ACFG_REQ_TX99TOOL](sc, (acfg_data_t *)&local);

	if(copy_to_user(ifr->ifr_data, &local, sizeof(struct acfg_tx99)))
		error = QDF_STATUS_E_NOMEM;

done:
	return __qdf_status_to_os(error);
}


int
__qdf_net_set_wifiaddr(struct net_device *netdev, void *addr)
{
	__qdf_softc_t      *sc      = netdev_to_softc(netdev);
	struct sockaddr    *user    = addr;
	acfg_macaddr_t      local   = {{0}};
	uint32_t          error   = QDF_STATUS_E_NOSUPPORT;

	memcpy(netdev->dev_addr, user->sa_data, ACFG_MACADDR_LEN);
	memcpy(local.addr, user->sa_data, ACFG_MACADDR_LEN);

	if (!sc2wifi_cfg(sc)->set_hwaddr) goto done;

	error = sc2wifi_cfg(sc)->set_hwaddr(sc->drv_hdl, &local);

done:
	return __qdf_status_to_os(error);
}



typedef int (* __linux_ioctl_fn_t)(struct net_device     *netdev,
								   struct ifreq   *ifr);

#define LINUX_PVT_WIOCTL        (SIOCWANDEV)


#define __DEV_IDX(x)          (x - LINUX_PVT_DEV_START)
#define __DEV_REQ(x)          [__DEV_IDX(LINUX_PVT_##x)]



/**
 * @brief WIFI IOCTL switch
 */
static const __linux_ioctl_fn_t   wifi_ioctl[LINUX_PVT_DEV_SZ] = {
	__DEV_REQ(GETATH_STATS)       = __qdf_net_wifi_getath_stats,      /* 0 */
	__DEV_REQ(CLRATH_STATS)       = __qdf_net_wifi_clrath_stats,      /* 4 */
	__DEV_REQ(PHYERR)             = __qdf_net_wifi_phyerr,            /* 5 */
	__DEV_REQ(VAP_CREATE)         = __qdf_net_wifi_create_vap,         /* 7 */
	__DEV_REQ(TX99TOOL)           = __qdf_net_wifi_tx99tool,           /* 13 */
};

/**
 * @brief Linux wifi specific Main IOCTL function
 *
 * @param netdev    (Netdevice Wifi or VAP)
 * @param ifr
 * @param cmd
 *
 * @return
 */
int
__qdf_net_wifi_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	if ((cmd >= LINUX_PVT_DEV_START) &&
		(cmd <= LINUX_PVT_DEV_END) &&
		wifi_ioctl[__DEV_IDX(cmd)])
		return wifi_ioctl[__DEV_IDX(cmd)](netdev, ifr);

	switch (cmd) {
		case LINUX_PVT_WIOCTL:
			return __qdf_net_wifi_wioctl(netdev, ifr);

		default:
			/* printk("%s:Unhandled IOCTL = 0x%x\n", __func__, cmd); */
			break;
	}

	qdf_assert(cmd != LINUX_PVT_GETCHAN_INFO);
	qdf_assert(cmd != LINUX_PVT_GETCHAN_LIST);

	return -ENOTSUPP;
}


/**
 * @brief Get P2P Big Param
 *
 * @param netdev
 * @param iwr
 *
 * @return
 */
static int
__get_p2p_big_param(struct net_device *netdev, struct iwreq *iwr, int param)
{
	__qdf_softc_t *sc = netdev_to_softc(netdev);
	acfg_p2p_param_t      local = {0};
	uint32_t            error = QDF_STATUS_E_NOMEM;

	local.param   = param;
	local.length  = iwr->u.data.length;
	local.pointer = vmalloc(local.length);
	if (!local.pointer) goto done;


	qdf_trace(QDF_DEBUG_FUNCTRACE,"%s(): Get VAP P2P Param. \n", __FUNCTION__);

	error = __wioctl_sw[ACFG_REQ_GET_VAP_P2P_PARAM](sc, (acfg_data_t *)&local);

	iwr->u.data.length = local.length;
	if (copy_to_user(iwr->u.data.pointer, local.pointer, local.length))
		error = QDF_STATUS_E_NOMEM;

	vfree(local.pointer);

done:
	return __qdf_status_to_os(error);
}


/**
 * @brief Set P2P Big Param
 *
 * @param netdev
 * @param iwr
 *
 * @return
 */
static int
__set_p2p_big_param(struct net_device *netdev, struct iwreq *iwr, int param)
{
	__qdf_softc_t   *sc = netdev_to_softc(netdev);
	acfg_p2p_param_t    local   = {0};
	uint32_t          error   = QDF_STATUS_E_NOMEM;

	local.param   = param;
	local.length  = iwr->u.data.length;
	local.pointer = vmalloc(local.length);
	if (!local.pointer) goto done;

	qdf_trace(QDF_DEBUG_FUNCTRACE,"%s(): Set VAP P2P Param. \n", __FUNCTION__);

	if (copy_from_user(local.pointer, iwr->u.data.pointer, local.length))
		goto freemem;

	error = __wioctl_sw[ACFG_REQ_SET_VAP_P2P_PARAM](sc, (acfg_data_t *)&local);

freemem:
	vfree(local.pointer);
done:
	return __qdf_status_to_os(error);
}
/**
 * @brief Get/Set P2P Big Param
 *
 * @param netdev
 * @param iwr
 *
 * @return
 */
int
__qdf_net_vap_p2p_bigparam(struct net_device *netdev, struct ifreq * ifr)
{
	struct iwreq    *iwr = (struct iwreq *)ifr;

	switch(iwr->u.data.flags) {
		case IEEE80211_IOC_P2P_FETCH_FRAME:
		case IEEE80211_IOC_P2P_NOA_INFO:
		case IEEE80211_IOC_P2P_FIND_BEST_CHANNEL:
			return __get_p2p_big_param(netdev, iwr, iwr->u.data.flags);

		case IEEE80211_IOC_SCAN_REQ:
		case IEEE80211_IOC_P2P_SEND_ACTION:
		case IEEE80211_IOC_P2P_SET_CHANNEL:
		case IEEE80211_IOC_P2P_GO_NOA:
			return __set_p2p_big_param(netdev, iwr, iwr->u.data.flags);
	}

	return -ENOTSUPP;
}


/**
 * @brief Get Scan Results
 *
 * @param netdev
 * @param iwr
 *
 * @return
 */
int
__qdf_net_vap_getscan_results(struct net_device *netdev, struct ifreq * ifr)
{
	__qdf_softc_t   *sc = netdev_to_softc(netdev);
	struct iwreq   *iwr     = (struct iwreq *)ifr;
	acfg_scan_t    *local   = NULL;
	void           *mem     = NULL;
	uint32_t      error   = QDF_STATUS_E_NOMEM;

	if (iwr->u.data.length < 0 ){
		printk("%s : Invalid Len\n",__FUNCTION__);
		error = QDF_STATUS_E_INVAL;
		goto done;
	}

	mem = vmalloc(sizeof(struct acfg_scan) + iwr->u.data.length);
	if(!mem) goto done;

	local           = mem;
	local->len      = iwr->u.data.length;
	local->results  = (uint8_t *)mem + sizeof(struct acfg_scan);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Get Scan Results\n", __FUNCTION__);

	error = __wioctl_sw[ACFG_REQ_GET_SCANRESULTS](sc, (acfg_data_t *)&local);

	iwr->u.data.length = local->len;
	if (copy_to_user (iwr->u.data.pointer, local->results, local->len))
		error = QDF_STATUS_E_NOMEM;

	vfree(mem);
done:
	return __qdf_status_to_os(error);
}

static int
__qdf_net_vap_delete( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t *sc = netdev_to_softc(netdev);
	acfg_vapinfo_t      local   = {0};
	uint32_t          error   = QDF_STATUS_E_FAILURE;

	memcpy(local.icp_name, ifr->ifr_ifrn.ifrn_name, ACFG_MAX_IFNAME);

	/* truncate */
	local.icp_name[ACFG_MAX_IFNAME] = '\0';

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s(): Vap Delete - %s \n",
			  __FUNCTION__, local.icp_name );

	error = __wioctl_sw[ACFG_REQ_DELETE_VAP](sc, (acfg_data_t *)&local);

	return __qdf_status_to_os(error) ;
}

/**
 * @brief Handle request issued by wlanconfig to list
 *        information on associated stations.
 *
 * @param netdev
 * @param ifr
 * @param cmd
 *
 * @return
 */
static int
__qdf_net_vap_getsta_info( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t *sc = netdev_to_softc(netdev);
	struct iwreq   *iwr     = (struct iwreq *)ifr;
	uint16_t        iwr_len = iwr->u.data.length + sizeof(uint32_t);
	void           *local   = NULL;
	uint32_t      error   = QDF_STATUS_E_NOMEM;

	if(!sc2vap_cfg(sc)->get_sta_info) return -ENOTSUPP ;

	local = vmalloc(iwr_len);
	if(!local) goto done;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s():assoc sta info len - %d\n",
			  __FUNCTION__, iwr_len);

	error = sc2vap_cfg(sc)->get_sta_info(sc->drv_hdl, local, iwr_len);

	iwr->u.data.length = *(uint32_t *)local;

	if (copy_to_user(iwr->u.data.pointer, ((uint32_t *)local + 1), iwr->u.data.length))
		error = QDF_STATUS_E_NOMEM;

	vfree(local);

done:
	return __qdf_status_to_os(error);
}



/**
 * @brief Handle request issued by wlanconfig to get
 *        channel info.
 *
 * @param netdev
 * @param ifr
 * @param cmd
 *
 * @return
 */
static int
__qdf_net_vap_getchan_info( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t      *sc      = netdev_to_softc(netdev);
	struct iwreq *iwr = (struct iwreq *)ifr ;
	acfg_chan_info_t   *local   = NULL;
	int                 error   = QDF_STATUS_E_NOMEM;

	if(!sc2vap_cfg(sc)->get_chan_info) return -ENOTSUPP ;

	local = vmalloc(sizeof(struct acfg_chan_info));
	if (!local) goto done;

	memset(local, 0, sizeof(struct acfg_chan_info));

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s():channel info\n", __FUNCTION__);

	error = sc2vap_cfg(sc)->get_chan_info(sc->drv_hdl, local);

	if (copy_to_user(iwr->u.data.pointer, local, iwr->u.data.length))
		error = QDF_STATUS_E_NOMEM;

	vfree(local);
done:
	return __qdf_status_to_os(error);
}



/**
 * @brief Handle request issued by wlanconfig to get
 *        channel list.
 *
 * @param netdev
 * @param ifr
 * @param cmd
 *
 * @return
 */
static int
__qdf_net_vap_getchan_list( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t  *sc      = netdev_to_softc(netdev);
	struct iwreq *iwr = (struct iwreq *)ifr ;
	acfg_opaque_t   local   = {0};
	uint32_t      error   = QDF_STATUS_E_NOMEM;

	if(!sc2vap_cfg(sc)->get_chan_list) return -ENOTSUPP ;

	local.len     = iwr->u.data.length;
	local.pointer = vmalloc(local.len);
	if (!local.pointer) goto done;

	memset(local.pointer, 0, local.len);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s():channel list for %d\n",
			  __FUNCTION__, local.len);

	error = sc2vap_cfg(sc)->get_chan_list(sc->drv_hdl, &local);

	if (copy_to_user(iwr->u.data.pointer, local.pointer, local.len))
		error = QDF_STATUS_E_NOMEM;

	vfree(local.pointer);
done:
	return __qdf_status_to_os(error);
}




static int
__qdf_net_vap_config_nawds( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t *sc = netdev_to_softc(netdev);
	acfg_nawds_cfg_t    local   = {0};
	uint32_t          error   = QDF_STATUS_E_NOMEM;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s():nawds_config\n", __FUNCTION__);

	if (copy_from_user(&local, ifr->ifr_data, sizeof(struct acfg_nawds_cfg)))
		goto done;

	error = __wioctl_sw[ACFG_REQ_NAWDS_CONFIG](sc, (acfg_data_t *)&local);

	if(copy_to_user(ifr->ifr_data, &local, sizeof(struct acfg_nawds_cfg)))
		error = QDF_STATUS_E_NOMEM;
done:
	return __qdf_status_to_os(error) ;
}

/**
 * @brief Handle request issued by hostapd to get wpa IE
 *        of associating STA
 *
 * @param netdev
 * @param ifr
 * @param cmd
 *
 * @return
 */
static int
__qdf_net_vap_getwpaie( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t  *sc      = netdev_to_softc(netdev);
	struct iwreq *iwr = (struct iwreq *)ifr ;
	uint16_t        iwr_len = iwr->u.data.length;
	void           *local   = NULL ;
	uint32_t      error   = QDF_STATUS_E_NOMEM;

	if(!sc2vap_cfg(sc)->get_wpa_ie) return -ENOTSUPP ;

	local = vmalloc(iwr_len);
	if(!local) goto done ;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s():station wpaie %d\n",
			  __FUNCTION__, iwr_len);

	if (copy_from_user(local, iwr->u.data.pointer, iwr_len))
		goto freemem;

	error = sc2vap_cfg(sc)->get_wpa_ie(sc->drv_hdl, local, iwr_len);

	if (copy_to_user(iwr->u.data.pointer, local, iwr_len))
		error = QDF_STATUS_E_NOMEM;

freemem:
	vfree(local);
done:
	return __qdf_status_to_os(error);
}



/**
 * @brief Get key for associated STA
 *
 * @param netdev
 * @param ifr
 * @param cmd
 *
 * @return
 */
static int
__qdf_net_vap_getkey( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t  *sc      = netdev_to_softc(netdev);
	struct iwreq *iwr = (struct iwreq *)ifr ;
	uint16_t        iwr_len = iwr->u.data.length;
	void           *local   = NULL;
	uint32_t      error   = QDF_STATUS_E_NOMEM;

	if(!sc2vap_cfg(sc)->get_key) return -ENOTSUPP ;

	local = vmalloc(iwr_len);
	if(!local) goto done;

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s():Get key len - %d \n",
			  __FUNCTION__, iwr_len);

	if (copy_from_user(local, iwr->u.data.pointer, iwr_len))
		goto freemem;

	error = sc2vap_cfg(sc)->get_key(sc->drv_hdl, local, iwr_len);

	if (copy_to_user(iwr->u.data.pointer, local, iwr_len))
		error = QDF_STATUS_E_NOMEM;

freemem:
	vfree(local);
done:
	return __qdf_status_to_os(error);
}


/**
 * @brief  Get STA Statistics
 *
 * @param netdev
 * @param ifr
 * @param cmd
 *
 * @return
 */
static int
__qdf_net_vap_getsta_stats( struct net_device *netdev, struct ifreq *ifr)
{
	__qdf_softc_t *sc = netdev_to_softc(netdev);
	struct iwreq   *iwr     = (struct iwreq *)ifr;
	uint16_t        iwr_len = iwr->u.data.length;
	void           *local   = NULL ;
	uint32_t      status  = QDF_STATUS_E_NOMEM;


	if(!sc2vap_cfg(sc)->get_sta_stats) return -ENOTSUPP ;

	local = vmalloc(iwr_len) ;
	if(!local) goto done;

	memset(local, 0, iwr_len);

	qdf_trace(QDF_DEBUG_FUNCTRACE, "%s():station stats len - %d\n",
			  __FUNCTION__, iwr_len);

	if (copy_from_user(local, iwr->u.data.pointer, iwr_len))
		goto freemem;

	status = sc2vap_cfg(sc)->get_sta_stats(sc->drv_hdl, local, iwr_len);

	if (copy_to_user(iwr->u.data.pointer, local, iwr_len))
	   status = QDF_STATUS_E_NOMEM;

freemem:
	vfree(local);
done:
	return __qdf_status_to_os(status);
}

int
__qdf_net_set_vapaddr(struct net_device *netdev, void *addr)
{
	struct sockaddr    *user    = addr;
	acfg_macaddr_t      local   = {{0}};

	memcpy(netdev->dev_addr, user->sa_data, ACFG_MACADDR_LEN);
	memcpy(local.addr, user->sa_data, ACFG_MACADDR_LEN);

	return __qdf_status_to_os(QDF_STATUS_SUCCESS);
}

/**
 * @brief VAP IOCTL switch
 */
static const __linux_ioctl_fn_t   vap_ioctl[LINUX_PVT_DEV_SZ] = {
	__DEV_REQ(GETKEY)           = __qdf_net_vap_getkey,             /* 3 */
	__DEV_REQ(GETWPAIE)         = __qdf_net_vap_getwpaie,           /* 4 */
	__DEV_REQ(GETSTA_STATS)     = __qdf_net_vap_getsta_stats,       /* 5 */
	__DEV_REQ(GETSTA_INFO)      = __qdf_net_vap_getsta_info,        /* 6 */
	__DEV_REQ(VAP_DELETE)       = __qdf_net_vap_delete,             /* 8 */
	__DEV_REQ(GETSCAN_RESULTS)  = __qdf_net_vap_getscan_results,    /* 9 */
	__DEV_REQ(CONFIG_NAWDS)     = __qdf_net_vap_config_nawds,       /* 12 */
	__DEV_REQ(P2P_BIG_PARAM)    = __qdf_net_vap_p2p_bigparam,       /* 14 */
};


int
__qdf_net_vap_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	if ((cmd >= LINUX_PVT_DEV_START) &&
		(cmd <= LINUX_PVT_DEV_END) &&
		vap_ioctl[__DEV_IDX(cmd)])
		return vap_ioctl[__DEV_IDX(cmd)](netdev, ifr);

	switch (cmd) {
		case LINUX_PVT_WIOCTL:
			return __qdf_net_vap_wioctl(netdev, ifr);

		default:
			/* printk("%s:Unhandled IOCTL = 0x%x\n", __func__, cmd); */
		   break;
	}

	qdf_assert(cmd != LINUX_PVT_GETCHAN_INFO);
	qdf_assert(cmd != LINUX_PVT_GETCHAN_LIST);

	return -ENOTSUPP;
}

/**
 * @brief Eth device IOCTL handler
 */
int
__qdf_net_eth_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
		default:
			printk("%s:Unhandled IOCTL = 0x%x\n", __func__, cmd);
		 break;
	}
	return -ENOTSUPP;
}
