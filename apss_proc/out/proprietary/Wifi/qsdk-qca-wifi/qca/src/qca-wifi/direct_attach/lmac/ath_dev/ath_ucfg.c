/*
 * Copyright (c) 2016, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* This is the unified configuration file for iw, acfg and netlink cfg, etc. */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */
#include <net/iw_handler.h>

#include <asm/uaccess.h>

#include "if_media.h"
#include "_ieee80211.h"
#include <wlan_opts.h>
#include <ieee80211_var.h>
#include <ieee80211_ioctl.h>
#include "ieee80211_rateset.h"
#include "ieee80211_vi_dbg.h"
#if ATH_SUPPORT_IBSS_DFS
#include <ieee80211_regdmn.h>
#endif

#include "if_athvar.h"
#include "if_athproto.h"

#include "ath_ucfg.h"
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_lmac_if_api.h>
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif
#if WLAN_SPECTRAL_ENABLE
#include <wlan_spectral_ucfg_api.h>
#endif

int ath_ucfg_setparam(void *vscn, int param, int value)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211*)vscn;
    struct ath_softc          *sc  =  ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah =   sc->sc_ah;
    int retval  = 0;
    struct ieee80211com *ic = NET80211_HANDLE(sc->sc_ieee);

    /*
    ** Code Begins
    ** Since the parameter passed is the value of the parameter ID, we can call directly
    */
    if ( param & ATH_PARAM_SHIFT )
    {
        /*
        ** It's an ATH value.  Call the  ATH configuration interface
        */

        param -= ATH_PARAM_SHIFT;
        retval = scn->sc_ops->ath_set_config_param(scn->sc_dev,
                (ath_param_ID_t)param,
                &value);
    }
    else if ( param & SPECIAL_PARAM_SHIFT )
    {
        if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_COUNTRY_ID) ) {
            if (sc->sc_ieee_ops->set_countrycode) {
                retval = sc->sc_ieee_ops->set_countrycode(
                        sc->sc_ieee, NULL, value, CLIST_NEW_COUNTRY);
            }
        } else if (param  == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_DISP_TPC) ) {
            ath_hal_display_tpctables(ah);
        } else if (param  == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_ENABLE_CH_144) ) {
            if (sc->sc_ieee_ops->modify_chan_144) {
                sc->sc_ieee_ops->modify_chan_144(sc->sc_ieee, value);
            }

            if (sc->sc_ieee_ops->get_channel_list_from_regdb) {
                sc->sc_ieee_ops->get_channel_list_from_regdb(sc->sc_ieee);
            }
        } else  if (param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_REGDOMAIN)) {
            uint16_t orig_rd = ieee80211_get_regdomain(sc->sc_ieee);
            if (sc->sc_ieee_ops->set_regdomaincode) {
                retval = sc->sc_ieee_ops->set_regdomaincode(sc->sc_ieee, value);
                if (retval) {
                    qdf_print("%s: Unable to set regdomain",__func__);
                    sc->sc_ieee_ops->set_regdomaincode(sc->sc_ieee, orig_rd);
                }
            }
        }
        else if (param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_ENABLE_SHPREAMBLE) ) {
            if (value) {
               ieee80211com_set_cap(ic, IEEE80211_C_SHPREAMBLE);
            } else {
               ieee80211com_clear_cap(ic, IEEE80211_C_SHPREAMBLE);
            }
        } else if (param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_ENABLE_MAC_REQ) ) {
            if ( !TAILQ_EMPTY(&ic->ic_vaps) ) {
                printk("%s: We do not support this if there are VAPs present\n",__func__);
                retval = -EBUSY;
            } else {
                scn->macreq_enabled = value;
                if (sc->sc_hasbmask) {
                    /* reset the bssid mask before setting the new mask */
                    OS_MEMSET(sc->sc_bssidmask, 0xff, sizeof(sc->sc_bssidmask));
                    if (scn->macreq_enabled)
                        ATH_SET_VAP_BSSID_MASK_ALTER(sc->sc_bssidmask);
                    else
                        ATH_SET_VAP_BSSID_MASK(sc->sc_bssidmask);

                    ath_hal_setbssidmask(sc->sc_ah, sc->sc_bssidmask);
                }
            }
        } else
            retval = -EOPNOTSUPP;
    }
    else
    {
        retval = (int) ath_hal_set_config_param(
                ah, (HAL_CONFIG_OPS_PARAMS_TYPE)param, &value);
    }

    return retval;
}
EXPORT_SYMBOL(ath_ucfg_setparam);

int ath_ucfg_getparam(void *vscn, int param, int *val)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211*)vscn;
    struct ath_softc          *sc   = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah   = sc->sc_ah;
    int retval  = 0;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct ieee80211com *ic = &scn->sc_ic;

    /*
    ** Code Begins
    ** Since the parameter passed is the value of the parameter ID, we can call directly
    */
    if ( param & ATH_PARAM_SHIFT )
    {
        /*
        ** It's an ATH value.  Call the  ATH configuration interface
        */

        param -= ATH_PARAM_SHIFT;
        if ( scn->sc_ops->ath_get_config_param(scn->sc_dev,(ath_param_ID_t)param,val) )
        {
            retval = -EOPNOTSUPP;
        }
    }
    else if ( param & SPECIAL_PARAM_SHIFT )
    {
        if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_COUNTRY_ID) ) {
            val[0] = sc->sc_ieee_ops->get_countrycode(sc->sc_ieee);
        } else if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_ENABLE_CH_144) ) {

            pdev =  ic->ic_pdev_obj;
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
                    QDF_STATUS_SUCCESS) {
                qdf_print("%s, %d unable to get reference", __func__, __LINE__);
                return -EINVAL;
            }

            psoc = wlan_pdev_get_psoc(pdev);
            if (psoc == NULL) {
                qdf_print("%s: psoc is NULL", __func__);
                return -EINVAL;
            }

            reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
            if (!(reg_rx_ops && reg_rx_ops->reg_get_chan_144)) {
                qdf_print("%s : reg_rx_ops is NULL", __func__);
                return -EINVAL;
            }

            val[0] = reg_rx_ops->reg_get_chan_144(pdev);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

        } else if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_REGDOMAIN) ) {
            val[0] = sc->sc_ieee_ops->get_regdomaincode(sc->sc_ieee);
        } else if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_ENABLE_SHPREAMBLE) ) {
            val[0] = (scn->sc_ic.ic_caps & IEEE80211_C_SHPREAMBLE) != 0;
        } else if ( param == (SPECIAL_PARAM_SHIFT | SPECIAL_PARAM_ENABLE_SHSLOT) ) {
            val[0] = IEEE80211_IS_SHSLOT_ENABLED(&scn->sc_ic) ? 1 : 0;
        } else {
            retval = -EOPNOTSUPP;
        }
    }
    else
    {
        if ( !ath_hal_get_config_param(ah, (HAL_CONFIG_OPS_PARAMS_TYPE)param, val) )
        {
            retval = -EOPNOTSUPP;
        }
    }

    return retval;
}
EXPORT_SYMBOL(ath_ucfg_getparam);

int ath_ucfg_set_countrycode(void *vscn, char *cntry)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211*)vscn;
    struct ath_softc *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    int retval;

    if (sc->sc_ieee_ops->set_countrycode) {
        retval = sc->sc_ieee_ops->set_countrycode(sc->sc_ieee, cntry, 0, CLIST_NEW_COUNTRY);
    } else {
        retval = -EOPNOTSUPP;
    }

    return retval;
}
EXPORT_SYMBOL(ath_ucfg_set_countrycode);

int ath_ucfg_get_country(void *vscn, char *str)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211*)vscn;
    struct ieee80211com *ic = &scn->sc_ic;

    ieee80211_getCurrentCountryISO(ic, str);
    return true;
}
EXPORT_SYMBOL(ath_ucfg_get_country);

#if ATH_SUPPORT_DSCP_OVERRIDE
void ath_ucfg_set_dscp_tid_map(void *vscn, u_int8_t tos, u_int8_t tid)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)vscn;
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);

    sc->sc_ieee_ops->set_dscp_tid_map(sc->sc_ieee, tos, tid);
}
EXPORT_SYMBOL(ath_ucfg_set_dscp_tid_map);

int ath_ucfg_get_dscp_tid_map(void *vscn, u_int8_t tos)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)vscn;
    struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);

    return sc->sc_ieee_ops->get_dscp_tid_map(sc->sc_ieee, tos);
}
EXPORT_SYMBOL(ath_ucfg_get_dscp_tid_map);
#endif

int ath_ucfg_set_mac_address(void *vscn, char *addr)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211*)vscn;
    struct ieee80211com *ic = &scn->sc_ic;
    struct net_device *dev = scn->sc_osdev->netdev;
    struct sockaddr sa;
    int retval;

    if ( !TAILQ_EMPTY(&ic->ic_vaps) ) {
        retval = -EBUSY; //We do not set the MAC address if there are VAPs present
    } else {
        IEEE80211_ADDR_COPY(&sa.sa_data, addr);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
        retval = dev->netdev_ops->ndo_set_mac_address(dev, &sa);
#else
        retval = dev->set_mac_address(dev, &sa);
#endif
    }

    return retval;
}
EXPORT_SYMBOL(ath_ucfg_set_mac_address);

#if UNIFIED_SMARTANTENNA
int ath_ucfg_set_smartantenna_param(void *vscn, char *val)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)vscn;
    struct wlan_objmgr_pdev *pdev;
    QDF_STATUS status;
    int ret = -1;

    pdev = scn->sc_pdev;
    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s, %d unable to get reference", __func__, __LINE__);
        return ret;
    }

    ret = target_if_sa_api_ucfg_set_param(pdev, val);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
    return ret;
}
EXPORT_SYMBOL(ath_ucfg_set_smartantenna_param);

int ath_ucfg_get_smartantenna_param(void *vscn, char *val)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)vscn;
    struct wlan_objmgr_pdev *pdev;
    QDF_STATUS status;
    int ret = -1;

    pdev = scn->sc_pdev;
    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s, %d unable to get reference", __func__, __LINE__);
        return ret;
    }

    ret = target_if_sa_api_ucfg_get_param(pdev, val);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
    return ret;
}
EXPORT_SYMBOL(ath_ucfg_get_smartantenna_param);
#endif

int ath_ucfg_diag(struct ath_softc_net80211 *scn, struct ath_diag *ad)
{
    struct ath_hal *ah = (ATH_DEV_TO_SC(scn->sc_dev))->sc_ah;
    u_int id = ad->ad_id & ATH_DIAG_ID;
    void *indata = NULL;
    void *outdata = NULL;
    u_int32_t insize = ad->ad_in_size;
    u_int32_t outsize = ad->ad_out_size;
    int error = 0;
    if (ad->ad_id & ATH_DIAG_IN) {
        /*
         * Copy in data.
         */
        indata = qdf_mem_malloc(insize);
        if (indata == NULL) {
            error = -ENOMEM;
            goto bad;
        }
        if (__xcopy_from_user(indata, ad->ad_in_data, insize)) {
            error = -EFAULT;
            goto bad;
        }
    }
    if (ad->ad_id & ATH_DIAG_DYN) {
        /*
         * Allocate a buffer for the results (otherwise the HAL
         * returns a pointer to a buffer where we can read the
         * results).  Note that we depend on the HAL leaving this
         * pointer for us to use below in reclaiming the buffer;
         * may want to be more defensive.
         */
        outdata = qdf_mem_malloc(outsize);
        if (outdata == NULL) {
            error = -ENOMEM;
            goto bad;
        }
    }
    if (ath_hal_getdiagstate(ah, id, indata, insize, &outdata, &outsize)) {
        printk("alloc size = %d, new = %d\n", ad->ad_out_size, outsize);
        if (outsize < ad->ad_out_size)
            ad->ad_out_size = outsize;
        if (outdata &&
                _copy_to_user(ad->ad_out_data, outdata, ad->ad_out_size))
            error = -EFAULT;
    } else {
        error = -EINVAL;
    }
bad:
    if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
        qdf_mem_free(indata);
    if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
        qdf_mem_free(outdata);
    return error;

}
EXPORT_SYMBOL(ath_ucfg_diag);

#if defined(ATH_SUPPORT_DFS) || defined(WLAN_SPECTRAL_ENABLE)
int ath_ucfg_phyerr(void *vscn, void *vad)
{
    void *indata=NULL;
    void *outdata=NULL;
    int error = -EINVAL;
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)vscn;
    struct ath_diag *ad = (struct ath_diag *)vad;
    u_int32_t insize = ad->ad_in_size;
    u_int32_t outsize = ad->ad_out_size;
    u_int id= ad->ad_id & ATH_DIAG_ID;
    struct ieee80211com *ic = &scn->sc_ic;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is NULL", __func__);
        return -EINVAL;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    if (psoc == NULL) {
        qdf_print("%s : psoc is NULL", __func__);
        return -EINVAL;
    }

    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);

    /*
       EV904010 -- Make sure there is a VAP on the interface
       before the request is processed
       */
    if ((ATH_DEV_TO_SC(scn->sc_dev))->sc_nvaps == 0) {
        return -EINVAL;
    }

    if (ad->ad_id & ATH_DIAG_IN) {
        /*
         * Copy in data.
         */
        indata = OS_MALLOC(scn->sc_osdev,insize, GFP_KERNEL);
        if (indata == NULL) {
            error = -ENOMEM;
            goto bad;
        }
        if (__xcopy_from_user(indata, ad->ad_in_data, insize)) {
            error = -EFAULT;
            goto bad;
        }
        id = id & ~ATH_DIAG_IN;
    }
    if (ad->ad_id & ATH_DIAG_DYN) {
        /*
         * Allocate a buffer for the results (otherwise the HAL
         * returns a pointer to a buffer where we can read the
         * results).  Note that we depend on the HAL leaving this
         * pointer for us to use below in reclaiming the buffer;
         * may want to be more defensive.
         */
        outdata = OS_MALLOC(scn->sc_osdev, outsize, GFP_KERNEL);
        if (outdata == NULL) {
            error = -ENOMEM;
            goto bad;
        }
        id = id & ~ATH_DIAG_DYN;
    }

    if (dfs_rx_ops && dfs_rx_ops->dfs_control) {
        if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return -EINVAL;
        }

        dfs_rx_ops->dfs_control(pdev, id, indata, insize, outdata, &outsize, &error);

        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
    }

#if WLAN_SPECTRAL_ENABLE
    if (error ==  -EINVAL ) {
        error = ucfg_spectral_control(
                ic->ic_pdev_obj, id, indata, insize, outdata, &outsize);
    }
#endif

    if (outsize < ad->ad_out_size)
        ad->ad_out_size = outsize;

    if (outdata &&
            _copy_to_user(ad->ad_out_data, outdata, ad->ad_out_size))
        error = -EFAULT;
bad:
    if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
        OS_FREE(indata);
    if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
        OS_FREE(outdata);

    return error;
}
EXPORT_SYMBOL(ath_ucfg_phyerr);
#endif
