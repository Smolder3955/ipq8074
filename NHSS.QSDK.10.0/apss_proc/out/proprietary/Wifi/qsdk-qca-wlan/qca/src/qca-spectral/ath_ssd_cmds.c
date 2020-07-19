/*
 * Copyright (c) 2014, 2017-2018 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2014 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * =====================================================================================
 *
 *       Filename:  ath_ssd_cmds.c
 *
 *    Description:  Spectral Scan commands (IOCTLs)
 *
 *        Version:  1.0
 *       Revision:  none
 *       Compiler:  gcc
 *
 * =====================================================================================
 */


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <netdb.h>
#include <net/if.h>

#include "if_athioctl.h"
#define  _LINUX_TYPES_H
#include "ath_classifier.h"
#include "spectral_ioctl.h"
#include "ath_ssd_defs.h"
#include "spectral_data.h"
#include "spec_msg_proto.h"
#include "spectral.h"
#include "nl80211_copy.h"
#include "cfg80211_nlwrapper_api.h"

#ifndef ATH_DEFAULT
#define ATH_DEFAULT "wifi0"
#endif

static int send_command (ath_ssd_info_t* pinfo, const char *ifname, void *buf, size_t buflen, int ioctl_sock_fd);

/**
 * send_command; function to send the cfg command or ioctl command.
 * @pinfo  : pointer to ath_ssd_info_t
 * @ifname : interface name
 * @buf    : buffer
 * @buflen : buffer length
 * return  : 0 for success
 */
int send_command (ath_ssd_info_t* pinfo, const char *ifname, void *buf, size_t buflen, int ioctl_sock_fd)
{

#ifdef SPECTRAL_SUPPORT_CFG80211
    struct cfg80211_data buffer;
    int nl_cmd = QCA_NL80211_VENDOR_SUBCMD_PHYERR;
    int msg;
    wifi_cfg80211_context *pcfg80211_sock_ctx;
#endif /* SPECTRAL_SUPPORT_CFG80211 */
    struct ifreq ifr;
    int ioctl_cmd = SIOCGATHPHYERR;

#ifdef SPECTRAL_SUPPORT_CFG80211
    pcfg80211_sock_ctx = GET_ADDR_OF_CFGSOCKINFO(pinfo);
    if (IS_CFG80211_ENABLED(pinfo)) {
        buffer.data = buf;
        buffer.length = buflen;
        buffer.callback = NULL;
        buffer.parse_data = 0;
        msg = wifi_cfg80211_sendcmd(pcfg80211_sock_ctx,
                        nl_cmd, ifname, (char *)&buffer, buflen);
        if (msg < 0) {
            fprintf(stderr, "Couldn't send NL command\n");
            return FAILURE;
        }
    } else
#endif /* SPECTRAL_SUPPORT_CFG80211 */
    {
        if (ifname) {
            memset(ifr.ifr_name, '\0', IFNAMSIZ);
            if (strlcpy(ifr.ifr_name, ifname, IFNAMSIZ) >= IFNAMSIZ) {
                fprintf(stderr, "ifname too long: %s\n", ifname);\
                return FAILURE;
            }
        } else {
            fprintf(stderr, "no such file or device\n");
            return FAILURE;
        }
        ifr.ifr_data = buf;
        if (ioctl(ioctl_sock_fd, ioctl_cmd, &ifr) < 0) {
            perror("ioctl failed");
            return FAILURE;
        }
    }

    return SUCCESS;
}

int get_vap_priv_int_param(ath_ssd_info_t* pinfo, const char *ifname, int param)
{
#if SPECTRAL_SUPPORT_CFG80211
    struct cfg80211_data buffer;
    wifi_cfg80211_context *pcfg80211_sock_ctx = NULL;
    uint32_t value;
    int msg;

    pcfg80211_sock_ctx = GET_ADDR_OF_CFGSOCKINFO(pinfo);
    if (IS_CFG80211_ENABLED(pinfo) && (pcfg80211_sock_ctx != NULL)) {
        buffer.data = &value;
        buffer.length = sizeof(value);
        buffer.parse_data = 0;
        buffer.callback = NULL;
        buffer.parse_data = 0;
        msg = wifi_cfg80211_send_getparam_command(pcfg80211_sock_ctx,
                QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS, param, ifname, (char *)&buffer, sizeof(uint32_t));
        /* we need to pass subcommand as well */
        if (msg < 0) {
            fprintf(stderr,"Couldn't send NL command\n");
            return -EIO;
        }
        return value;
    }
#endif /* SPECTRAL_SUPPORT_CFG80211 */
    return -EINVAL;
}

/*
 * Function     : ath_ssd_init_spectral
 * Description  : initialize spectral related info
 * Input params : pointer to ath_ssd_info_t info structrue
 * Return       : success/failure
 *
 */
int ath_ssd_init_spectral(ath_ssd_info_t* pinfo)
{
    int err = SUCCESS;
    ath_ssd_spectral_info_t* psinfo = &pinfo->sinfo;

    if (pinfo->radio_ifname == NULL)
        return -EINVAL;

    if (strlcpy(psinfo->atd.ad_name, pinfo->radio_ifname, sizeof(psinfo->atd.ad_name)) >= sizeof(psinfo->atd.ad_name)) {
        fprintf(stderr, "radio_ifname too long: %s\n", pinfo->radio_ifname);
        return FAILURE;
    }

    return err;
}


/*
 * Function     : start_spectral_scan
 * Description  : start the spectrla scan on current channel
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int ath_ssd_start_spectral_scan(ath_ssd_info_t* pinfo)
{
    u_int32_t status = SUCCESS;
    ath_ssd_nlsock_t *pnlinfo = GET_ADDR_OF_NLSOCKINFO(pinfo);

    ath_ssd_spectral_info_t* psinfo = &pinfo->sinfo;

    psinfo->atd.ad_id = SPECTRAL_ACTIVATE_SCAN | ATH_DIAG_DYN;
    psinfo->atd.ad_in_data = NULL;
    psinfo->atd.ad_in_size = 0;
    psinfo->atd.ad_out_data = (void*)&status;
    psinfo->atd.ad_out_size = sizeof(u_int32_t);
    if (send_command(pinfo, psinfo->atd.ad_name, (caddr_t)&psinfo->atd, sizeof(struct ath_diag),
                    pnlinfo->spectral_fd) != SUCCESS) {
        status= FAILURE;
    }
    return status;
}

/*
 * Function     : stop_spectral_scan
 * Description  : stop the spectrla scan on current channel
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int ath_ssd_stop_spectral_scan(ath_ssd_info_t* pinfo)
{
    u_int32_t status = SUCCESS;

    ath_ssd_spectral_info_t* psinfo = &pinfo->sinfo;
    ath_ssd_nlsock_t *pnlinfo = GET_ADDR_OF_NLSOCKINFO(pinfo);

    /* XXX : We should set priority to 0 */

    psinfo->atd.ad_id = SPECTRAL_STOP_SCAN | ATH_DIAG_DYN;
    psinfo->atd.ad_in_data = NULL;
    psinfo->atd.ad_in_size = 0;
    psinfo->atd.ad_out_data = (void*)&status;
    psinfo->atd.ad_out_size = sizeof(u_int32_t);

    if(send_command(pinfo, psinfo->atd.ad_name, (caddr_t)&psinfo->atd, sizeof(struct ath_diag),
                     pnlinfo->spectral_fd) != SUCCESS) {
        status = FAILURE;
    }
    return status;
}

/*
 * Function     : set_spectral_param
 * Description  : set spectral param
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int ath_ssd_set_spectral_param(ath_ssd_info_t* pinfo, int op, int param)
{
    int status = SUCCESS;
    SPECTRAL_PARAMS_T sp;
    ath_ssd_nlsock_t *pnlinfo = GET_ADDR_OF_NLSOCKINFO(pinfo);
    ath_ssd_spectral_info_t* psinfo = &pinfo->sinfo;

    ath_ssd_get_spectral_param(pinfo, &sp);

    switch(op) {
        case SPECTRAL_PARAM_FFT_PERIOD:
            sp.ss_fft_period = param;
            break;
        case SPECTRAL_PARAM_SCAN_PERIOD:
            sp.ss_period = param;
            break;
        case SPECTRAL_PARAM_SHORT_REPORT:
            {
                if (param) {
                    sp.ss_short_report = 1;
                } else {
                    sp.ss_short_report = 0;
                }
            }
            break;
        case SPECTRAL_PARAM_SCAN_COUNT:
            sp.ss_count = param;
            break;

        case SPECTRAL_PARAM_SPECT_PRI:
            sp.ss_spectral_pri = (!!param) ? true:false;
            break;

        case SPECTRAL_PARAM_FFT_SIZE:
            sp.ss_fft_size = param;
            break;

        case SPECTRAL_PARAM_GC_ENA:
            sp.ss_gc_ena = !!param;
            break;

        case SPECTRAL_PARAM_RESTART_ENA:
            sp.ss_restart_ena = !!param;
            break;

        case SPECTRAL_PARAM_NOISE_FLOOR_REF:
            sp.ss_noise_floor_ref = param;
            break;

        case SPECTRAL_PARAM_INIT_DELAY:
            sp.ss_init_delay = param;
            break;

        case SPECTRAL_PARAM_NB_TONE_THR:
            sp.ss_nb_tone_thr = param;
            break;

        case SPECTRAL_PARAM_STR_BIN_THR:
            sp.ss_str_bin_thr = param;
            break;

        case SPECTRAL_PARAM_WB_RPT_MODE:
            sp.ss_wb_rpt_mode = !!param;
            break;

        case SPECTRAL_PARAM_RSSI_RPT_MODE:
            sp.ss_rssi_rpt_mode = !!param;
            break;

        case SPECTRAL_PARAM_RSSI_THR:
            sp.ss_rssi_thr = param;
            break;

        case SPECTRAL_PARAM_PWR_FORMAT:
            sp.ss_pwr_format = !!param;
            break;

        case SPECTRAL_PARAM_RPT_MODE:
            sp.ss_rpt_mode = param;
            break;

        case SPECTRAL_PARAM_BIN_SCALE:
            sp.ss_bin_scale = param;
            break;

        case SPECTRAL_PARAM_DBM_ADJ:
            sp.ss_dBm_adj = !!param;
            break;

        case SPECTRAL_PARAM_CHN_MASK:
            sp.ss_chn_mask = param;
            break;
    }

    psinfo->atd.ad_id = SPECTRAL_SET_CONFIG | ATH_DIAG_IN;
    psinfo->atd.ad_out_data = NULL;
    psinfo->atd.ad_out_size = 0;
    psinfo->atd.ad_in_data = (void *) &sp;
    psinfo->atd.ad_in_size = sizeof(SPECTRAL_PARAMS_T);

    if (send_command(pinfo, psinfo->atd.ad_name, (caddr_t)&psinfo->atd, sizeof(struct ath_diag),
                     pnlinfo->spectral_fd) != SUCCESS) {
        status = FAILURE;
    }

    return status;
}

void ath_ssd_get_spectral_param(ath_ssd_info_t* pinfo, SPECTRAL_PARAMS_T* sp)
{

    ath_ssd_nlsock_t *pnlinfo = GET_ADDR_OF_NLSOCKINFO(pinfo);
    ath_ssd_spectral_info_t* psinfo = &pinfo->sinfo;

    psinfo->atd.ad_id = SPECTRAL_GET_CONFIG | ATH_DIAG_DYN;
    psinfo->atd.ad_out_data = (void *)sp;
    psinfo->atd.ad_out_size = sizeof(SPECTRAL_PARAMS_T);

    send_command(pinfo, psinfo->atd.ad_name, (caddr_t)&psinfo->atd, sizeof(struct ath_diag),
                     pnlinfo->spectral_fd);
    return;
}

/*
 * Function     : get_channel_width
 * Description  : Get current channel width from driver
 * Input params : pointer to icm info structrue
 * Return       : success/failure
 *
 */
int get_channel_width(ath_ssd_info_t* pinfo)
{
    u_int32_t ch_width = 0;
    ath_ssd_nlsock_t *pnlinfo = GET_ADDR_OF_NLSOCKINFO(pinfo);

    ath_ssd_spectral_info_t* psinfo = &pinfo->sinfo;

    psinfo->atd.ad_id = SPECTRAL_GET_CHAN_WIDTH | ATH_DIAG_DYN;
    psinfo->atd.ad_in_data = NULL;
    psinfo->atd.ad_in_size = 0;
    psinfo->atd.ad_out_data = (void*)&ch_width;
    psinfo->atd.ad_out_size = sizeof(u_int32_t);

    if (send_command(pinfo, psinfo->atd.ad_name, (caddr_t)&psinfo->atd, sizeof(struct ath_diag),
                     pnlinfo->spectral_fd) != SUCCESS) {
        ch_width = IEEE80211_CWM_WIDTHINVALID;
    }
    return ch_width;
}

/*
 * Function     : ath_ssd_get_spectral_capabilities
 * Description  : Get Spectral capabilities
 * Input params : pointer to ath_ssd_info_t
 * Output params: pointer to struct spectral_caps which is to be populated in
 *                case of SUCCESS.
 * Return       : SUCCESS/FAILURE
 */
int ath_ssd_get_spectral_capabilities(ath_ssd_info_t* pinfo,
        struct spectral_caps *caps)
{
    ath_ssd_nlsock_t *pnlinfo = NULL;
    ath_ssd_spectral_info_t* psinfo = NULL;

    ATHSSD_ASSERT(pinfo != NULL);
    ATHSSD_ASSERT(caps != NULL);

    pnlinfo = GET_ADDR_OF_NLSOCKINFO(pinfo);
    psinfo = &pinfo->sinfo;

    memset(caps, 0, sizeof(*caps));

    psinfo->atd.ad_id = SPECTRAL_GET_CAPABILITY_INFO | ATH_DIAG_DYN;
    psinfo->atd.ad_out_data = (void *)caps;
    psinfo->atd.ad_out_size = sizeof(*caps);

    if (send_command(pinfo, psinfo->atd.ad_name, (caddr_t)&psinfo->atd,
                sizeof(struct ath_diag), pnlinfo->spectral_fd) != SUCCESS) {
        return FAILURE;
    }

    return SUCCESS;
}

