/*
 * Copyright (c) 2016-2019 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2016 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * =====================================================================================
 *
 *       Filename:  icm_wal.c
 *
 *    Description:  Intelligent Channel Manager
 *
 *        Version:  1.0
 *        Created:  04/19/2012 01:17:17 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  S.Karthikeyan (),
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/wait.h>
#include "icm.h"
#include "icm_api.h"

#ifdef ICM_RTR_DRIVER
#include "ieee80211_external.h"
#include "ath_classifier.h"
#endif /* ICM_RTR_DRIVER */

#include "icm_wal.h"


#ifdef WLAN_SPECTRAL_ENABLE
/*
 * Function     : icm_wal_is_spectral_enab
 * Description  : check is spectral is enabled
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_is_spectral_enab(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_is_spectral_enab(picm);
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_is_spectral_enab(picm);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }

    return 0;
}

/*
 * Function     : icm_wal_get_spectral_params
 * Description  : Get values of Spectral parameters
 * Input params : pointer to icm info structure
 * Output params: pointer to Spectral params structure, to be populated
 * Return       : SUCCESS/FAILURE
 */
int icm_wal_get_spectral_params(ICM_INFO_T *picm, SPECTRAL_PARAMS_T *sp)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_get_spectral_params(picm, sp);
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_get_spectral_params(picm, sp);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }

    return 0;

}
/*
 * Function     : icm_wal_set_spectral_params
 * Description  : Set values of Spectral parameters
 * Input params : pointer to icm info structure, pointer to Spectral params
 *                structure containing values to be set
 * Return       : SUCCESS/FAILURE
 */
int icm_wal_set_spectral_params(ICM_INFO_T *picm, SPECTRAL_PARAMS_T *sp)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_set_spectral_params(picm, sp);
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_set_spectral_params(picm, sp);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }

    return 0;
}

/*
 * Function     : icm_wal_get_spectral_capabilities
 * Description  : Get Spectral capabilities
 * Input params : pointer to icm info structure
 * Output params: pointer to Spectral capabilities structure, to be populated
 * Return       : SUCCESS/FAILURE
 */
int icm_wal_get_spectral_capabilities(ICM_INFO_T *picm,
        struct spectral_caps *scaps)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    ICM_ASSERT(picm != NULL);
    ICM_ASSERT(scaps != NULL);

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return FAILURE;
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_get_spectral_capabilities(picm, scaps);
#else
        return SUCCESS;
#endif
    } else {
        return FAILURE;
    }

    return 0;

}

/*
 * Function     : icm_wal_start_spectral_scan
 * Description  : start the spectrla scan on current channel
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_start_spectral_scan(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_start_spectral_scan(picm);
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_start_spectral_scan(picm);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_stop_spectral_scan
 * Description  : stop the spectrla scan on current channel
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_stop_spectral_scan(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_stop_spectral_scan(picm);
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_stop_spectral_scan(picm);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_set_spectral_debug
 * Description  : set the spectral module debug level
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_set_spectral_debug(ICM_INFO_T* picm, int dbglevel)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_set_spectral_debug(picm, dbglevel);
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_set_spectral_debug(picm, dbglevel);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_clear_spectral_chan_properties
 * Description  : clean the spectral channel properties
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_clear_spectral_chan_properties(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_clear_spectral_chan_properties(picm);
    } else if (pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}
#endif /* WLAN_SPECTRAL_ENABLE */

/*
 * Function     : icm_wal_get_channel_vendorsurvey_info
 * Description  : get channel vendor survey information
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_get_channel_vendorsurvey_info(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef WLAN_SPECTRAL_ENABLE
        return icm_wal_ioctl_get_channel_vendorsurvey_info(picm);
#endif /* WLAN_SPECTRAL_ENABLE */
    } else if (pconf->walflag == ICM_WAL_CFG) {
        /* Dont't need this, equivalent to chan_info event from driver */
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_set_icm_active
 * Description  : set the spectral module debug level
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_set_icm_active(ICM_INFO_T *picm, u_int32_t val)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef WLAN_SPECTRAL_ENABLE
        return icm_wal_ioctl_set_icm_active(picm, val);
#endif /* WLAN_SPECTRAL_ENABLE */
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        icm_wal_rtrcfg_set_icm_active(picm, val);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_get_nominal_noisefloor
 * Description  : get nominal noisefloor
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_get_nominal_noisefloor(ICM_INFO_T *picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef WLAN_SPECTRAL_ENABLE
        return icm_wal_ioctl_get_nominal_noisefloor(picm);
#endif /* WLAN_SPECTRAL_ENABLE */
    } else if (pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_get_nominal_noisefloor(picm);
#else
        return SUCCESS;
#endif
    } else {
        return -1;
    }
    return 0;
}

void icm_wal_init_channel_params(ICM_INFO_T * picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        icm_wal_ioctl_init_channel_params(picm);
#endif /* ICM_RTR_DRIVER */
    } else if (pconf->walflag == ICM_WAL_CFG) {
        icm_wal_cfg_init_channel_params(picm);
    }
}

/*
 * Function     : icm_wal_do_80211_scan
 * Description  : do an 802.11 scan and print scan results
 * Input params : pointer to icm
 * Return       : 0 on success, -1 on general errors, -2 on scan cancellation 
 */

int icm_wal_do_80211_scan(ICM_INFO_T * picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_do_80211_scan(picm);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return icm_wal_mblcfg_do_80211_scan(picm);
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_cancel_80211_scan
 * Description  : Cancel all 802.11 scans for the given icm
 * Input params : pointer to icm
 * Return       : success/failure
 */
int icm_wal_cancel_80211_scan(ICM_INFO_T * picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_cancel_80211_scan(picm);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return icm_wal_mblcfg_cancel_80211_scan(picm);
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_do_80211_priv
 * Description  : interface to 80211 priv ioctl
 * Input params : pointer to icm, channel number, pointer to
 *                iwreq, interface name
 * Return       : success/failure
 */

    int
icm_wal_do_80211_priv(ICM_INFO_T *picm, struct iwreq *iwr, const char *ifname, int op, void *data, size_t len)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_do_80211_priv(picm, iwr, ifname, op, data, len);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_get_currdomain
 * Description  : Obtain the current RF regulatory domain being used by
 *                the radio and place it in the rfreg_domain of the
 *                ICM_INFO_T object.
 * Input params : pointer to icm
 * Return       : 0 on success, -1 on failure
 */
int icm_wal_get_currdomain(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_get_currdomain(picm);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return icm_wal_mblcfg_get_currdomain(picm);
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_get_regdomain
 * Description  : Obtain the  RF regulatory domain being used by
 *                the radio and place it in the rfreg_domain of the
 *                ICM_INFO_T object.
 * Input params : pointer to icm
 * Return       : 0 on success, -1 on failure
 */
int icm_wal_get_reg_domain(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_get_reg_domain(picm);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return icm_wal_mblcfg_get_reg_domain(picm);
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_get_currchan
 * Description  : Obtain the current channel information
 * Input params : pointer to icm
 * Return       : 0 on success, -1 on failure
 */
int icm_wal_get_currchan(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_get_currchan(picm);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        //return icm_wal_mblcfg_get_currchan(picm);
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_get_ieee_chaninfo
 * Description  : prints ieee related channel information
 * Input params : pointer to ieee80211_ath_channel
 * Return       : success/failure
 */
int icm_wal_get_ieee_chaninfo(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_get_ieee_chaninfo(picm);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_get_paramrange
 * Description  : gets parameter range information
 * Input params : pointer to icm
 * Output params: pointer to iw_range structure
 * Return       : success/failure
 */
int icm_wal_get_paramrange(ICM_INFO_T *picm, struct iw_range *range)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_get_paramrange(picm, range);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_ioctl_is_dev_up
 * Description  : find whether a device is up
 * Input params : pointer to ICM_DEV_INFO_T, device name
 * Out params   : pointer to bool indicating whether device is
 up (valid only on SUCCESS)
 * Return       : SUCCESS/FAILURE
 */

int icm_wal_is_dev_up(ICM_DEV_INFO_T* pdev,
        char *dev_ifname,
        bool *isup)
{
    ICM_CONFIG_T* pconf = NULL;
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_is_dev_up(pdev, dev_ifname, isup);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_is_dev_ap
 * Description  : find whether a device is in AP mode
 * Input params : pointer to ICM_DEV_INFO_T, device name
 * Out params   : pointer to bool indicating whether device is
 in AP mode (valid only on SUCCESS)
 * Return       : SUCCESS/FAILURE
 */
int icm_wal_is_dev_ap(ICM_DEV_INFO_T* pdev,
        char *dev_ifname,
        bool *isap)
{
    ICM_CONFIG_T* pconf = NULL;
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_is_dev_ap(pdev, dev_ifname, isap);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_get_radio_priv_int_param
 * Description  : Get a radio-private integer parameter
 * Input params : pointer to pdev info, radio interface name, required parameter,
 *                pointer to return value buffer
 * Return       : On success: 0
 *                On error  : -1
 */
int icm_wal_get_radio_priv_int_param(ICM_DEV_INFO_T* pdev, const char *ifname, int param,
                                     int32_t *val)
{
    ICM_CONFIG_T* pconf = NULL;
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_get_radio_priv_int_param(pdev, ifname, param, val);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_set_radio_priv_int_param
 * Description  : Set a radio-private integer parameter
 * Input params : pointer to pdev info, radio interface name, required parameter,
 *                value
 * Return       : On success: 0
 *                On error  : -1
 */
int icm_wal_set_radio_priv_int_param(ICM_DEV_INFO_T* pdev, const char *ifname, int param, int val)
{
    ICM_CONFIG_T* pconf = NULL;
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_set_radio_priv_int_param(pdev, ifname, param, val);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_get_vap_priv_int_param
 * Description  : Return private parameter of the given VAP from driver.
 * Input params : const char pointer pointing to interface name and required parameter
 * Return       : Success: value of the private param
 *                Failure: -1
 *
 */
int icm_wal_get_vap_priv_int_param(ICM_DEV_INFO_T* pdev,
        const char *ifname,
        int param)
{
    ICM_CONFIG_T* pconf = NULL;
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_get_vap_priv_int_param(pdev, ifname, param);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
        /* Dont't need this */
    } else {
        return -1;
    }
    return 0;
}
/*
 * Function     : icm_wal_set_vap_priv_int_param
 * Description  : Set a device-private integer parameter
 * Input params : pointer to pdev info, device interface name, parameter,
 *                value.
 * Return       : On success: 0
 *                On error  : -1
 */
int icm_wal_set_vap_priv_int_param(ICM_DEV_INFO_T* pdev,
        const char *ifname,
        int param,
        int32_t val)
{
    ICM_CONFIG_T* pconf = NULL;
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_set_vap_priv_int_param(pdev, ifname, param, val);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
        /* Dont't need this */
    } else {
        return -1;
    }
    return 0;
}
#ifdef WLAN_SPECTRAL_ENABLE
/*
 * Function     : icm_wal_get_channel_width
 * Description  : Get current channel width from driver
 * Input params : pointer to icm info structure
 * Return       : Channel width on success
 *                IEEE80211_CWM_WIDTHINVALID on failure
 */
enum ieee80211_cwm_width icm_wal_get_channel_width(ICM_INFO_T* picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        return icm_wal_ioctl_get_channel_width(picm);
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return SUCCESS;
    } else {
        return -1;
    }
    return 0;
}
#endif /* WLAN_SPECTRAL_ENABLE */

/*
 * Function     : icm_wal_set_width_and_channel 
 * Description  : set width and channel as per best channel
 *                selection done previously.
 *                It is the caller's responsibility to ensure
 *                that the best channel selection has already been
 *                carried out, or if this has not been done, then
 *                a default channel has been set instead.
 *                It is the best channel selection code's
 *                responsibility to ensure that the width and channel
 *                are correct.
 * Input params : pointer to icm info, device name
 * Return       : SUCCESS/FAILURE
 */

int icm_wal_set_width_and_channel(ICM_INFO_T *picm, 
        char *dev_ifname)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_ioctl_set_width_and_channel(picm, dev_ifname);
#endif /* ICM_RTR_DRIVER */
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return icm_wal_mblcfg_set_width_and_channel(picm, dev_ifname);
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_set_channel
 * Description  : Set channel to driver
 * Input params : pointer to icm info structure
 * Return       : success/failure
 */
int icm_wal_set_channel(ICM_INFO_T* picm, struct nl80211_channel_config *chan_config)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        /* Dont't need this */
        return SUCCESS;
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return icm_wal_mblcfg_set_channel(picm, chan_config);
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_get_chan_rropinfo
 * Description  : Get Representative RF Operating Parameter (RROP) information
 * Input params : pointer to icm
 * Return       : 0 on success, -1 on general errors
 */
int icm_wal_get_chan_rropinfo(ICM_INFO_T * picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        /* This is not required for WEXT */
        return SUCCESS;
    } else if(pconf->walflag == ICM_WAL_CFG) {
        return icm_wal_mblcfg_get_chan_rropinfo(picm);
    } else {
        return -1;
    }
    return 0;
}

/*
 * Function     : icm_wal_get_radio_pri20_blockchanlist
 * Description  : Get radio level primary 20 MHz channel block list
 * Input params : pointer to icm structure
 * Return       : 0 on success, -1 on error
 */
int icm_wal_get_radio_pri20_blockchanlist(ICM_INFO_T * picm)
{
    ICM_CONFIG_T* pconf = NULL;
    ICM_DEV_INFO_T* pdev = get_pdev();
    pconf = &pdev->conf;

    if (pconf->walflag == ICM_WAL_IOCTL) {
        /* This is not supported for WEXT */
        return SUCCESS;
    } else if(pconf->walflag == ICM_WAL_CFG) {
#ifdef ICM_RTR_DRIVER
        return icm_wal_rtrcfg_get_radio_pri20_blockchanlist(picm);
#else
        /* This is not required for MBL config */
        return SUCCESS
#endif /* ICM_RTR_DRIVER */
    } else {
        return -1;
    }
    return 0;
}
