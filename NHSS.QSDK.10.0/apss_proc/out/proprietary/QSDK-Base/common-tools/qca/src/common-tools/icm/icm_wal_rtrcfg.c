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
 *       Filename:  icm_wal_rtrcfg.c
 *
 *    Description:  ICM WAL IOCTL related changes
 *
 *        Version:  1.0
 *        Created:  04/19/2012 01:18:58 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  S.Karthikeyan (),
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <icm.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "if_athioctl.h"
#define _LINUX_TYPES_H

#include "icm.h"
#include "icm_wal.h"
#include "spectral_ioctl.h"
#include "spectral_data.h"
#include "spec_msg_proto.h"
#include "ath_classifier.h"
#include "spectral_ioctl.h"
#include "spectral.h"
#include "ieee80211_external.h"
#include "icm_api.h"
#include "icm_wal.h"
#include "spectral_data.h"
#include "driver_nl80211.h"

#ifndef ATH_DEFAULT
#define ATH_DEFAULT "wifi0"
#endif

#ifdef WLAN_SPECTRAL_ENABLE
/*
 * Function     : icm_wal_rtrcfg_is_spectral_enab
 * Description  : check is spectral is enabled
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_rtrcfg_is_spectral_enab(ICM_INFO_T* picm)
{
    int ret;
    ICM_SPECTRAL_INFO_T* psinfo = &picm->sinfo;
    struct nl80211_spectral_scan_state status;

    memset(&status, 0, sizeof(status));
    ret = driver_nl80211_spectral_get_status(picm, psinfo->atd.ad_name,
                                                &status);
    if (ret < 0) {
        icm_printf("Spectral is enabled command failed\n");
        return -1;
    }

    if (status.is_enabled)
        return SUCCESS;
    else
        return FAILURE;
}

/*
 * Function     : icm_wal_rtrcfg_get_spectral_params
 * Description  : Get values of Spectral parameters
 * Input params : pointer to icm info structure
 * Output params: pointer to Spectral params structure, to be populated
 * Return       : SUCCESS/FAILURE
 */
int icm_wal_rtrcfg_get_spectral_params(ICM_INFO_T *picm, SPECTRAL_PARAMS_T *sp)
{
    int ret;
    ICM_SPECTRAL_INFO_T* psinfo = &picm->sinfo;

    ret = driver_nl80211_get_spectral_params(picm, psinfo->atd.ad_name, sp);
    if (ret < 0) {
        icm_printf("Spectral get params command failed\n");
        return FAILURE;
    }

    return SUCCESS;
}

/*
 * Function     : icm_wal_rtrcfg_get_spectral_capabilities
 * Description  : Get Spectral capabilities
 * Input params : pointer to icm info structure
 * Output params: pointer to Spectral capabilities structure, to be populated
 * Return       : SUCCESS/FAILURE
 */
int icm_wal_rtrcfg_get_spectral_capabilities(ICM_INFO_T *picm,
        struct spectral_caps *scaps)
{
    int ret;
    ICM_SPECTRAL_INFO_T* psinfo = NULL;

    ICM_ASSERT(picm != NULL);
    ICM_ASSERT(scaps != NULL);

    psinfo = &picm->sinfo;

    ret = driver_nl80211_get_spectral_capabilities(picm, psinfo->atd.ad_name,
            scaps);
    if (ret < 0) {
        icm_printf("Spectral get capabilities command failed\n");
        return FAILURE;
    }

    return SUCCESS;
}

/*
 * Function     : icm_wal_rtrcfg_set_spectral_params
 * Description  : Set values of Spectral parameters
 * Input params : pointer to icm info structure, pointer to Spectral params
 *                structure containing values to be set
 * Return       : SUCCESS/FAILURE
 */
int icm_wal_rtrcfg_set_spectral_params(ICM_INFO_T *picm, SPECTRAL_PARAMS_T *sp)
{
    int ret;
    ICM_SPECTRAL_INFO_T* psinfo = &picm->sinfo;

    if (sp == NULL) {
        err("icm: Spectral Parameters structure is invalid");
        return FAILURE;
    }

    ret = driver_nl80211_set_spectral_params(picm, psinfo->atd.ad_name, sp);
    if (ret < 0) {
        icm_printf("Spectral set params command failed\n");
        return FAILURE;
    }

    return SUCCESS;
}

/*
 * Function     : icm_wal_rtrcfg_start_spectral_scan
 * Description  : start the spectral scan on current channel
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_rtrcfg_start_spectral_scan(ICM_INFO_T* picm)
{
    int ret;
    ICM_SPECTRAL_INFO_T* psinfo = &picm->sinfo;

    ret = driver_nl80211_start_spectral_scan(picm, psinfo->atd.ad_name);
    if (ret < 0) {
        icm_printf("Spectral scan start command failed\n");
        return FAILURE;
    }
    picm->substate = ICM_STATE_SPECTRAL_SCAN;

    return SUCCESS;
}

/*
 * Function     : icm_wal_rtrcfg_set_spectral_debug
 * Description  : set the spectral module debug level
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_rtrcfg_set_spectral_debug(ICM_INFO_T* picm, int dbglevel)
{
    int ret;
    ICM_SPECTRAL_INFO_T* psinfo = &picm->sinfo;

    ret = driver_nl80211_set_spectral_debug(picm, psinfo->atd.ad_name, dbglevel);
    if (ret < 0) {
        icm_printf("Spectral set debug level command failed\n");
        return FAILURE;
    }

    return SUCCESS;
}


/*
 * Function     : icm_wal_rtrcfg_stop_spectral_scan
 * Description  : stop the spectrla scan on current channel
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_rtrcfg_stop_spectral_scan(ICM_INFO_T* picm)
{
    int ret;
    ICM_SPECTRAL_INFO_T* psinfo = &picm->sinfo;

    ret = driver_nl80211_stop_spectral_scan(picm, psinfo->atd.ad_name);
    if (ret < 0) {
        icm_printf("Spectral stop scan command failed\n");
        return FAILURE;
    }

    return SUCCESS;
}
#endif /* WLAN_SPECTRAL_ENABLE */


/*
 * Function     : icm_wal_rtrcfg_set_icm_active
 * Description  : set icm active
 * Input params : pointer to icm info structrue and value
 * Return       : success/failure
 *
 */
int icm_wal_rtrcfg_set_icm_active(ICM_INFO_T *picm, u_int32_t val)
{
    int ret = 0;

    ret = icm_wal_rtrcfg_set_radio_priv_int_param(picm,
                                                 picm->radio_ifname,
                                                 OL_ATH_PARAM_ICM_ACTIVE | OL_ATH_PARAM_SHIFT,
                                                 val);
    if (ret < 0) {
        fprintf(stderr, "%-8.16s  Could not set icm active value %d\n\n",
                picm->radio_ifname,
                val);
        return FAILURE;
    }

    return SUCCESS;
}

/*
 * Function     : icm_wal_rtrcfg_get_nominal_noisefloor
 * Description  : get nominal noisefloor
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int icm_wal_rtrcfg_get_nominal_noisefloor(ICM_INFO_T *picm)
{
    int ret = 0;
    int32_t nominal_nf =  ATH_DEFAULT_NOMINAL_NF;

    ret = icm_wal_rtrcfg_get_radio_priv_int_param(picm,
            picm->radio_ifname,
            OL_ATH_PARAM_NOMINAL_NOISEFLOOR | OL_ATH_PARAM_SHIFT,
            &nominal_nf);
    if (ret < 0) {
        nominal_nf = ATH_DEFAULT_NOMINAL_NF;
        perror("icm : cfg80211 command failure (OL_ATH_PARAM_NOMINAL_NOISEFLOOR)");
        return FAILURE;
    }

    return nominal_nf;
}

/*
 * Function     : icm_wal_ioclt_set_radio_priv_int_param
 * Description  : Set a device-private integer parameter
 * Input params : pointer to pdev info, device interface name, parameter,
 *                value.
 * Return       : On success: 0
 *                On error  : -1
 */
int icm_wal_rtrcfg_set_radio_priv_int_param(ICM_INFO_T* picm,
                                            const char *ifname,
                                            int param,
                                            int32_t val)
{
    return driver_set_wifi_priv_int_param(picm, ifname, param, val);
}

/*
 * Function     : icm_wal_rtrcfg_get_radio_priv_int_param
 * Description  : Get a radio-private integer parameter
 * Input params : pointer to pdev info, radio interface name, required parameter,
 *                pointer to value
 * Return       : On success: Value of parameter
 *                On error  : -1
 */
int icm_wal_rtrcfg_get_radio_priv_int_param(ICM_INFO_T* picm,
                                            const char *ifname,
                                            int param,
                                            int32_t *value)
{
    return driver_get_wifi_priv_int_param(picm, ifname, param, value);
}

/*
 * Function     : icm_wal_rtrcfg_get_radio_pri20_blockchanlist
 * Description  : Get radio level primary 20 MHz channel block list
 * Input params : pointer to icm structure
 * Return       : On success: 0
 *                On error  : -1
 */
int icm_wal_rtrcfg_get_radio_pri20_blockchanlist(ICM_INFO_T* picm)
{
    ICM_ASSERT(picm != NULL);

    return driver_get_wifi_pri20_blockchanlist(picm, picm->radio_ifname,
                &(picm->pri20_blockchanlist));
}
