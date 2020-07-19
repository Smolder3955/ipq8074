/*
 * Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/netdevice.h>
#include <net/iw_handler.h>

#include <asm/uaccess.h>

#include "if_media.h"
#include "_ieee80211.h"
#include <osif_private.h>

#include "if_athvar.h"
#include "if_athproto.h"

#include <qdf_types.h>
#include <acfg_api_types.h>
#include <acfg_drv_cfg.h>

#include  "ieee80211_acfg.h"
#include  "ieee80211_ioctl_acfg.h"
#include <linux/time.h>

#include "ath_ucfg.h"

static int
acfg_vap_create(void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ieee80211_clone_params cp;
    acfg_vapinfo_t    *ptr;
    acfg_os_req_t   *req = NULL;
    int status = 0;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    char name[IFNAMSIZ];

    req = (acfg_os_req_t *) buffer;
    ptr     = &req->data.vap_info;

    memset(&cp , 0, sizeof(cp));

    cp.icp_opmode = ptr->icp_opmode;
    cp.icp_flags  = ptr->icp_flags;
    cp.icp_vapid  = ptr->icp_vapid;
    memcpy(&cp.icp_name[0], &ptr->icp_name[0], ACFG_MAX_IFNAME);

    status = ath_ucfg_create_vap(scn, &cp, name);

    return status;
}

static int
acfg_set_wifi_param(void *ctx, uint16_t cmdid,
                   uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    acfg_param_req_t        *ptr;
    acfg_os_req_t   *req = NULL;
    int param, value;

    req = (acfg_os_req_t *) buffer;
    ptr  = &req->data.param_req;
    param = ptr->param;
    value = ptr->val;

    return ath_ucfg_setparam((void*)scn, param, value);
    return 0;
}

static int
acfg_get_wifi_param(void *ctx, uint16_t cmdid,
                   uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    acfg_param_req_t  *ptr;
    acfg_os_req_t     *req = NULL;
    int param, *val;

    req = (acfg_os_req_t *) buffer;
    ptr  = &req->data.param_req;
    val = &ptr->val;
    param = ptr->param;

    return ath_ucfg_getparam(scn, param, val);
    return 0;
}

extern void acfg_convert_to_acfgprofile (struct ieee80211_profile *profile,
                                    acfg_radio_vap_info_t *acfg_profile);
static int
acfg_get_profile (void *ctx, uint16_t cmdid,
               uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    acfg_os_req_t   *req = NULL;
    acfg_radio_vap_info_t *ptr;
    struct ieee80211_profile *profile;
    int error = 0;

    profile = (struct ieee80211_profile *)qdf_mem_malloc(
            sizeof (struct ieee80211_profile));
    if (profile == NULL) {
        return -ENOMEM;
    }
    OS_MEMSET(profile, 0, sizeof (struct ieee80211_profile));

    req = (acfg_os_req_t *) buffer;
    ptr     = &req->data.radio_vap_info;
    memset(ptr, 0, sizeof (acfg_radio_vap_info_t));

    error = ath_ucfg_get_vap_info(scn, profile);

    acfg_convert_to_acfgprofile(profile, ptr);

    if (profile != NULL) {
        qdf_mem_free(profile);
        profile = NULL;
    }

    return error;
}

static int
acfg_set_hw_addr(void *ctx, uint16_t cmdid,
                   uint8_t *buffer, int32_t Length)
{
    struct net_device *dev = (struct net_device *) ctx;
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    char *paddr = NULL;
    acfg_os_req_t *req = (acfg_os_req_t *) buffer;
    paddr  = (char *)&req->data.macaddr;

    return ath_ucfg_set_mac_address(scn, paddr);
}

acfg_dispatch_entry_t ath_acfgdispatchentry[] =
{
    { 0,                      ACFG_REQ_FIRST_WIFI        , 0 }, //--> 0
    { acfg_vap_create,        ACFG_REQ_CREATE_VAP        , 0 },
    { acfg_set_wifi_param,    ACFG_REQ_SET_RADIO_PARAM   , 0 },
    { acfg_get_wifi_param,    ACFG_REQ_GET_RADIO_PARAM   , 0 },
    { acfg_set_hw_addr,       ACFG_REQ_SET_HW_ADDR       , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { 0,                      ACFG_REQ_UNUSED            , 0 },
    { acfg_get_profile,       ACFG_REQ_GET_PROFILE       , 0 },
    { 0,                      ACFG_REQ_LAST_WIFI         , 0 }, //--> 11
};

int
acfg_handle_ath_ioctl(struct net_device *dev, void *data)
{
    acfg_os_req_t   *req = NULL;
    uint32_t    req_len = sizeof(struct acfg_os_req);
    int32_t status = 0;
    uint32_t i;
    bool cmd_found = false;
    req = qdf_mem_malloc(req_len);
    if (!req)
        return -ENOMEM;

    if (copy_from_user(req, data, req_len))
        goto done;

    qdf_debug("the cmd id is %d",req->cmd);
    /* Iterate the dispatch table to find the handler
     * for the command received from the user */
    for (i = 0; i < sizeof(ath_acfgdispatchentry)/sizeof(ath_acfgdispatchentry[0]); i++)
    {
        if (ath_acfgdispatchentry[i].cmdid == req->cmd) {
            cmd_found = true;
            break;
       }
    }

    if (cmd_found == false) {
        status = -1;
        qdf_print("ACFG ioctl CMD %d failed ",req->cmd);
        goto done;
    }

    if (ath_acfgdispatchentry[i].cmd_handler == NULL) {
        status = -1;
        qdf_print("ACFG ioctl CMD %d failed", ath_acfgdispatchentry[i].cmdid);
        goto done;
    }

    status = ath_acfgdispatchentry[i].cmd_handler(dev,
                    req->cmd, (uint8_t *)req, req_len);
    if (copy_to_user(data, req, req_len)) {
        qdf_print("copy_to_user error");
        status = -1;
    }
done:
    qdf_mem_free(req);
    return status;
}

