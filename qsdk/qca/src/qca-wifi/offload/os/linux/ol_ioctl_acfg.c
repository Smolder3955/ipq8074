/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2010, Qualcomm Atheros Communications Inc.
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
#include "dbglog_host.h"
#include "ol_regdomain.h"

#define __ACFG_PHYMODE_STRINGS__
#include <i_qdf_types.h>
#include <acfg_api_types.h>
#include <acfg_drv_cfg.h>
#include <acfg_drv_cmd.h>
#include <acfg_drv_event.h>

#include "ol_ath_ucfg.h"

extern int ol_ath_target_start(struct ol_ath_soc_softc *soc);
extern void acfg_convert_to_acfgprofile (struct ieee80211_profile *profile,
                                            acfg_radio_vap_info_t *acfg_profile);
extern QDF_STATUS
ol_ath_get_wmi_target_type(ol_ath_soc_softc_t *soc,
                           enum wmi_target_type *target);

int
ol_acfg_vap_create(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
	struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211_clone_params cp;
    acfg_vapinfo_t    *ptr;
    acfg_os_req_t   *req = NULL;
    char name[IFNAMSIZ];
    int ret;

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_vapinfo_t *)req->data;

    memset(&cp , 0, sizeof(cp));

    cp.icp_opmode = ptr->icp_opmode;
    cp.icp_flags  = ptr->icp_flags;
    cp.icp_vapid  = ptr->icp_vapid;

    memcpy(&cp.icp_name[0], &ptr->icp_name[0], ACFG_MAX_IFNAME);

    ret = ol_ath_ucfg_create_vap(scn, &cp, name);
    return ret;
}

int
ol_acfg_set_wifi_param(void *ctx, uint16_t cmdid,
                   uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    acfg_param_req_t  *ptr;
    acfg_os_req_t     *req = NULL;
    int param, value;

    req = (acfg_os_req_t *) buffer;
    ptr  = (acfg_param_req_t *)req->data;

    param = ptr->param;
    value = ptr->val;
    if (atomic_read(&scn->soc->reset_in_progress)) {
        qdf_print("Reset in progress, return");
        return -1;
    }

    return ol_ath_ucfg_setparam((void*)scn, param, value);
}

int
ol_acfg_get_wifi_param(void *ctx, uint16_t cmdid,
                   uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    acfg_param_req_t  *ptr;
    acfg_os_req_t     *req = NULL;
    int param, *val;

    req = (acfg_os_req_t *) buffer;
    ptr  = (acfg_param_req_t *)req->data;

    val = &ptr->val;
    param = ptr->param;

    return ol_ath_ucfg_getparam((void*)scn, param, val);
}

int
ol_acfg_set_reg (void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
    return -1;
}

int
ol_acfg_get_reg (void *ctx, uint16_t cmdid,
            uint8_t *buffer, int32_t Length)
{
    return -1;
}

int
ol_acfg_tx99tool (void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    int cmd;
    char *data;

    cmd  = *((int *)&req->data);
    data = (char *)(((int *)&req->data)+1);

    /*last parameter is length which is used only for FTM daemon*/
    return ol_ath_ucfg_utf_unified_cmd(scn, cmd, data, 0);
}

int
ol_acfg_set_hwaddr (void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    char *paddr;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    int status = ENOTSUPP;
    paddr = req->data;

    status = ol_ath_ucfg_set_mac_address((void*)scn, paddr);

    return status;
}

int
ol_acfg_get_profile (void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    acfg_os_req_t   *req = NULL;
    acfg_radio_vap_info_t *ptr;
    struct ieee80211_profile *profile;
    int error = 0;

    if (scn->soc->down_complete) {
        qdf_print("Target stopped: cannot get wifi param");
        return -1;
    }

    req = (acfg_os_req_t *) buffer;
    ptr     = (acfg_radio_vap_info_t *)req->data;
    memset(ptr, 0, sizeof (acfg_radio_vap_info_t));

    profile = (struct ieee80211_profile *)qdf_mem_malloc(
                    sizeof (struct ieee80211_profile));
    if (profile == NULL) {
        return -ENOMEM;
    }

    error = ol_ath_ucfg_get_vap_info(scn, profile);

    if(profile != NULL) {
        acfg_convert_to_acfgprofile(profile, ptr);
        qdf_mem_free(profile);
    }

    return error;
}

int ol_ath_get_nf_dbr_dbm_info(void *ctx, uint16_t cmdid,
                                  uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);

    return ol_ath_ucfg_get_nf_dbr_dbm_info(scn);
}

int ol_ath_get_packet_power_info(void *ctx, uint16_t cmdid,
                                  uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct packet_power_info_params *ptr;

    ptr = (struct packet_power_info_params *)req->data;

    return ol_ath_ucfg_get_packet_power_info(scn, ptr);
}

/* GPIO config routine */
int ol_ath_gpio_config(void *ctx, uint16_t cmdid,
                                  uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);

    acfg_gpio_config_t *ptr;
    ptr = (acfg_gpio_config_t *)req->data;

    printk("%s[%d] gpio_num %d input %d pull_type %d, intr_mode %d\n", __func__,__LINE__, ptr->gpio_num, ptr->input, ptr->pull_type, ptr->intr_mode);
    return ol_ath_ucfg_gpio_config((void*)scn, ptr->gpio_num, ptr->input, ptr->pull_type, ptr->intr_mode);
}

/* GPIO set routine */
int ol_ath_gpio_set(void *ctx, uint16_t cmdid,
                                  uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);

    acfg_gpio_set_t *ptr;
    ptr = (acfg_gpio_set_t *)req->data;

    printk("%s[%d] gpio_num %d set %d\n", __func__,__LINE__, ptr->gpio_num, ptr->set);
    if(ol_ath_ucfg_gpio_output((void*)scn, ptr->gpio_num, ptr->set)) {
        printk("%s:Unable to set GPIO\n", __func__);
        return -1;
    }
    return 0;
}

/* Set CTL power table */
int ol_ath_acfg_ctl_set(void *ctx, uint16_t cmdid,
                                  uint8_t *buffer, int32_t Length)
{
    acfg_os_req_t   *req = (acfg_os_req_t *)buffer;
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_ctl_table_t *ptr;

    ptr = (acfg_ctl_table_t *)req->data;

    return ol_ath_ucfg_ctl_set(scn, (ath_ctl_table_t *)ptr);
}

/**
 * @brief set basic & supported rates in beacon,
 * and use lowest basic rate as mgmt mgmt/bcast/mcast rates by default.
 */
static int
ol_acfg_set_op_support_rates(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_rateset_t  *target_rs = (acfg_rateset_t  *)req->data;

    return ol_ath_ucfg_set_op_support_rates(scn, (struct ieee80211_rateset *)target_rs);
}

/**
 * @brief get physically supported rates of the specified phymode for the radio
 */
static int
ol_acfg_get_radio_supported_rates(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_rateset_phymode_t *rs_phymode = (acfg_rateset_phymode_t *)req->data;
    acfg_rateset_t  *target_rs= &(rs_phymode->rs);
    enum ieee80211_phymode mode = rs_phymode->phymode;

    return ol_ath_ucfg_get_radio_supported_rates(scn, mode, (struct ieee80211_rateset *)target_rs);
}


static int
ol_acfg_set_bss_color(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_bss_color_t *bss_color_t = (acfg_bss_color_t *)req->data;

    return ol_ath_ucfg_set_he_bss_color(scn, bss_color_t->bsscolor, bss_color_t->override);
}

static int
ol_acfg_get_bss_color(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    acfg_bss_color_t *bss_color_t = (acfg_bss_color_t *)req->data;


    return ol_ath_ucfg_get_he_bss_color(scn, (int *)&(bss_color_t->bsscolor));
}

static int
ol_acfg_get_wmi_tgt_type(void *ctx, uint16_t cmdid,
             uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    uint8_t status;
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    enum wmi_target_type *wmi_tgt_type = (enum wmi_target_type *)req->data;

    status = ol_ath_get_wmi_target_type(scn->soc, wmi_tgt_type);
    if (status == QDF_STATUS_E_INVAL)
        *wmi_tgt_type = -1;
    return status;
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
    { 0,                         ACFG_REQ_GET_ATHSTATS      , 0 },
    { 0,                         ACFG_REQ_CLR_ATHSTATS      , 0 },
    { ol_acfg_get_profile,       ACFG_REQ_GET_PROFILE       , 0 },
    { 0,                         ACFG_REQ_PHYERR            , 0 },
    { ol_ath_gpio_config,        ACFG_REQ_GPIO_CONFIG       , 0 },
    { ol_ath_gpio_set,           ACFG_REQ_GPIO_SET          , 0 },
    { ol_ath_acfg_ctl_set,       ACFG_REQ_SET_CTL_TABLE     , 0 },
    { ol_acfg_set_op_support_rates,   ACFG_REQ_SET_OP_SUPPORT_RATES   , 0 },
    { ol_acfg_get_radio_supported_rates, ACFG_REQ_GET_RADIO_SUPPORTED_RATES   , 0 },
    { ol_ath_get_nf_dbr_dbm_info, ACFG_REQ_GET_NF_DBR_DBM_INFO   , 0 },
    { ol_ath_get_packet_power_info, ACFG_REQ_GET_PACKET_POWER_INFO   , 0 },
    { ol_acfg_set_bss_color,     ACFG_REQ_SET_BSS_COLOR   , 0 },
    { ol_acfg_get_bss_color,     ACFG_REQ_GET_BSS_COLOR   , 0 },
    { ol_acfg_get_wmi_tgt_type,  ACFG_REQ_GET_TLV_TYPE    , 0 },
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

    req = (acfg_os_req_t *)data;

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
            qdf_print("OL ACFG ioctl CMD %d failed", req->cmd);
            goto done;
        }
        status = ol_acfgdispatchentry[i].cmd_handler(dev,
                        req->cmd, (uint8_t *)req, req_len);
    }
    else
    {
        *isvap_ioctl = 1;
        qdf_print("%s: req->cmd=%d not valid for radio interface, it's for VAP",
                  __func__,req->cmd);
    }
done:
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

