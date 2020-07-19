/*
 * Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <net/if_arp.h>
#include <math.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <acfg_types.h>
#include <stdint.h>
#include <acfg_api_pvt.h>
#include <acfg_security.h>
#include <acfg_misc.h>
#include <acfg_api_event.h>
#include <linux/un.h>
#include <linux/netlink.h>

#include <appbr_if.h>
#include <acfg_wireless.h>

struct nlmsghdr *nlh = NULL;

//#define LINUX_PVT_WIOCTL  (SIOCDEVPRIVATE + 1)
#define SIOCWANDEV  0x894A
#define LINUX_PVT_WIOCTL  (SIOCWANDEV)

extern appbr_status_t appbr_if_send_cmd_remote(uint32_t app_id, void *buf,
        uint32_t size);

extern appbr_status_t appbr_if_wait_for_response(void *buf, uint32_t size,
        uint32_t timeout);

extern uint32_t acfg_wpa_supplicant_get(acfg_wlan_profile_vap_params_t *vap_params);

extern uint32_t acfg_hostapd_get(acfg_wlan_profile_vap_params_t *vap_params);

extern void acfg_send_interface_event(char *event, int len);

uint32_t acfg_set_radio_profile(acfg_wlan_profile_radio_params_t
        *radio_params, acfg_wlan_profile_radio_params_t *cur_radio_params);

struct socket_context g_sock_ctx;

uint32_t
acfg_get_err_status(void)
{
    switch (errno)  {
        case ENOENT:        return QDF_STATUS_E_NOENT;
        case ENOMEM:        return QDF_STATUS_E_NOMEM;
        case EINVAL:        return QDF_STATUS_E_INVAL;
        case EINPROGRESS:   return QDF_STATUS_E_ALREADY;
        case EBUSY:         return QDF_STATUS_E_BUSY;
        case E2BIG:         return QDF_STATUS_E_E2BIG;
        case ENXIO:         return QDF_STATUS_E_ENXIO;
        case EFAULT:        return QDF_STATUS_E_FAULT;
        case EIO:           return QDF_STATUS_E_IO;
        case EEXIST:        return QDF_STATUS_E_EXISTS;
        case ENETDOWN:      return QDF_STATUS_E_NETDOWN;
        case EADDRNOTAVAIL: return QDF_STATUS_E_ADDRNOTAVAIL;
        case ENETRESET:     return QDF_STATUS_E_NETRESET;
        case EOPNOTSUPP:    return QDF_STATUS_E_NOSUPPORT;
        default:            return QDF_STATUS_E_FAILURE;
    }
}

uint32_t
acfg_os_send_req(uint8_t *ifname, acfg_os_req_t  *req)
{
    int ret = QDF_STATUS_E_FAILURE;

    ret = send_command(&g_sock_ctx, (const char *)ifname, req, sizeof(acfg_os_req_t),
         NULL, QCA_NL80211_VENDOR_SUBCMD_ACFG, LINUX_PVT_WIOCTL);

    if (ret < 0) {
        acfg_log_errstr("%s failed, ret=%d!\n",__FUNCTION__,ret);
        return QDF_STATUS_E_FAILURE;
    } else {
        return QDF_STATUS_SUCCESS;
    }
}

/**
 * @brief  Initialize interface for device-less configurations
 *
 * @return
 */
uint32_t acfg_dl_init()
{

    uint32_t ret_status = QDF_STATUS_E_FAILURE;

    ret_status = appbr_if_open_dl_conn(APPBR_ACFG);
    if(ret_status != QDF_STATUS_SUCCESS)
        goto out;

    ret_status = appbr_if_open_ul_conn(APPBR_ACFG);
    if(ret_status != QDF_STATUS_SUCCESS)
        goto out;

out:
    return  ret_status;
}

/**
 * @brief Check whether string crossed max limit
 *
 * @param src
 * @param maxlen
 *
 * @return
 */
uint32_t
acfg_os_check_str(uint8_t *src, uint32_t maxlen)
{
    return(strnlen((const char *)src, maxlen) >= maxlen);
}

/**
 * @brief Compare two strings
 *
 * @param str1
 * @param str2
 * @param maxlen
 *
 * @return 0 if strings are same.
 *         Non zero otherwise.
 */
uint32_t
acfg_os_cmp_str(uint8_t *str1, uint8_t *str2, uint32_t maxlen)
{
    return(strncmp((const char *)str1, (const char *)str2, maxlen));
}


/**
 * @brief Copy the dst string into the src
 *
 * @param src (the source string)
 * @param dst (destination string)
 * @param maxlen (the maximum length of dest buf)
 *
 * @note It's assumed that the destination string is
 *       zero'ed
 */
uint32_t
acfg_os_strcpy(uint8_t  *dst, uint8_t *src, uint32_t  maxlen)
{
    uint32_t  len;
    len = strlcpy((char *)dst, (const char *)src, maxlen);
    return len;
}

uint32_t
acfg_set_chainmask(uint8_t *radio_name, enum acfg_chainmask_type type, uint16_t mask)
{
    uint32_t   status = QDF_STATUS_SUCCESS;

    switch (type) {
    case ACFG_TX_CHAINMASK:
        status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_TXCHAINMASK, mask);
        break;
    case ACFG_RX_CHAINMASK:
        status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_RXCHAINMASK, mask);
        break;
    default:
        break;
    }

    return status;
}

uint32_t
acfg_get_chainmask(uint8_t *radio_name, enum acfg_chainmask_type type, uint32_t *mask)
{
    uint32_t   status = QDF_STATUS_SUCCESS;
    uint32_t   val;

    switch (type) {
    case ACFG_TX_CHAINMASK:
        status = acfg_get_radio_param(radio_name, ACFG_PARAM_RADIO_TXCHAINMASK, &val);
        break;
    case ACFG_RX_CHAINMASK:
        status = acfg_get_radio_param(radio_name, ACFG_PARAM_RADIO_RXCHAINMASK, &val);
        break;
    default:
        break;
    }

    if (status == QDF_STATUS_SUCCESS) {
        *mask = val;
    }

    return status;
}

/*
 *  Public API's
 */


/**
 * @brief Create VAP
 *
 * @param wifi_name
 * @param vap_name
 * @param mode
 *
 * @return
 */
uint32_t
acfg_create_vap(uint8_t             *wifi_name,
        uint8_t             *vap_name,
        acfg_opmode_t          mode,
        int32_t              vapid,
        uint32_t             flags)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_CREATE_VAP};
    acfg_vapinfo_t    *ptr;

    ptr     = (acfg_vapinfo_t *)req.data;

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    acfg_os_strcpy((uint8_t *)ptr->icp_name, vap_name, ACFG_MAX_IFNAME);

    ptr->icp_opmode    = mode;
    ptr->icp_flags     = flags;
    ptr->icp_vapid     = vapid;

    status = acfg_os_send_req(wifi_name, &req);

    return status;
}


/**
 * @brief Delete VAP
 *
 * @param wifi_name
 * @param vap_name
 *
 * @return
 */
uint32_t
acfg_delete_vap(uint8_t *wifi_name,
        uint8_t *vap_name)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_DELETE_VAP};
    acfg_vapinfo_t    *ptr;

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME)
            || acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    ptr     = (acfg_vapinfo_t *)req.data;

    acfg_os_strcpy((uint8_t *)ptr->icp_name, vap_name, ACFG_MAX_IFNAME);

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

uint32_t
acfg_set_tx_antenna(uint8_t *radio_name,  uint16_t mask)
{
    return acfg_set_chainmask(radio_name, ACFG_TX_CHAINMASK, mask);
}

uint32_t
acfg_set_rx_antenna(uint8_t *radio_name,  uint16_t mask)
{
    return acfg_set_chainmask(radio_name, ACFG_RX_CHAINMASK, mask);
}

uint32_t
acfg_get_tx_antenna(uint8_t *radio_name,  uint32_t *mask)
{
    return acfg_get_chainmask(radio_name, ACFG_TX_CHAINMASK, mask);
}

uint32_t
acfg_get_rx_antenna(uint8_t *radio_name,  uint32_t *mask)
{
    return acfg_get_chainmask(radio_name, ACFG_RX_CHAINMASK, mask);
}

uint32_t
acfg_config_radio(uint8_t *radio_name)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;

    /* set all radio defaults here */

    status = acfg_set_chainmask(radio_name, ACFG_TX_CHAINMASK, 0x7);
    if (status != QDF_STATUS_SUCCESS)
        return status;

    status = acfg_set_chainmask(radio_name, ACFG_RX_CHAINMASK, 0x7);
    if (status != QDF_STATUS_SUCCESS)
        return status;

    status = acfg_set_txpower_limit(radio_name, ACFG_BAND_2GHZ, 63);
    if (status != QDF_STATUS_SUCCESS)
        return status;

    status = acfg_set_txpower_limit(radio_name, ACFG_BAND_5GHZ, 63);
    if (status != QDF_STATUS_SUCCESS)
        return status;

    status = acfg_set_country(radio_name, 841);
    return status;
}

/**
 * @brief Is the VAP local or remote
 *
 * @param vap_name
 *
 * @return
 */
uint32_t
acfg_is_offload_vap(uint8_t *vap_name)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_IS_OFFLOAD_VAP};
    //acfg_vapinfo_t    *ptr;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    //ptr     = &req.data.vap_info;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

uint32_t
acfg_add_client(uint8_t *vap_name, uint8_t *mac, uint32_t aid, uint32_t qos,
                acfg_rateset_t lrates, acfg_rateset_t htrates, acfg_rateset_t vhtrates,
                acfg_rateset_t herates)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_ADD_CLIENT};
    acfg_add_client_t    *ptr;

    ptr     = (acfg_add_client_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    memcpy(ptr->stamac, mac, ACFG_MACADDR_LEN);
    ptr->aid    = aid;
    ptr->qos    = qos;
    memcpy(&ptr->lrates, &lrates, sizeof(acfg_rateset_t));
    memcpy(&ptr->htrates, &htrates, sizeof(acfg_rateset_t));
    memcpy(&ptr->vhtrates, &vhtrates, sizeof(acfg_rateset_t));
    memcpy(&ptr->herates, &herates, sizeof(acfg_rateset_t));

    status = acfg_os_send_req(vap_name, &req);
    return status;
}

uint32_t
acfg_delete_client(uint8_t *vap_name, uint8_t *mac)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_DEL_CLIENT};
    acfg_del_client_t    *ptr;

    ptr     = (acfg_del_client_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    memcpy(ptr->stamac, mac, ACFG_MACADDR_LEN);
    status = acfg_os_send_req(vap_name, &req);

    return status;
}

uint32_t
acfg_forward_client(uint8_t *vap_name, uint8_t *mac)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_AUTHORIZE_CLIENT};
    acfg_authorize_client_t    *ptr;

    ptr     = (acfg_authorize_client_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    memcpy(ptr->mac, mac, ACFG_MACADDR_LEN);
    ptr->authorize = 1;

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

uint32_t
acfg_del_key(uint8_t *vap_name, uint8_t *mac, uint16_t keyidx)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_DEL_KEY};
    acfg_del_key_t    *ptr;

    ptr     = (acfg_del_key_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    memcpy(ptr->macaddr, mac, ACFG_MACADDR_LEN);
    ptr->keyix = keyidx;

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

uint32_t
acfg_set_key(uint8_t *vap_name, uint8_t *mac, CIPHER_METH cipher, uint16_t keyidx,
             uint32_t keylen, uint8_t *keydata)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_SET_KEY};
    acfg_set_key_t    *ptr;

    ptr     = (acfg_set_key_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    memcpy(ptr->macaddr, mac, ACFG_MACADDR_LEN);
    ptr->cipher = cipher;
    ptr->keylen = keylen;
    ptr->keyix = keyidx;
    memcpy(ptr->keydata, keydata, sizeof(ptr->keydata));

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

/**
 * @brief Set the SSID
 *
 * @param vap_name
 * @param ssid
 *
 * @return
 */
uint32_t
acfg_set_ssid(uint8_t     *vap_name, acfg_ssid_t  *ssid)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_SSID};
    acfg_ssid_t        *ptr;

    ptr     = (acfg_ssid_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    ptr->len = acfg_os_strcpy((uint8_t *)ptr->ssid, (uint8_t *)ssid->ssid, ACFG_MAX_SSID_LEN + 1);

    status = acfg_os_send_req(vap_name, &req);
    return status;
}

/**
 * @brief Get the SSID
 *
 * @param vap_name
 * @param ssid
 */
uint32_t
acfg_get_ssid(uint8_t  *vap_name, acfg_ssid_t  *ssid)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_SSID};
    acfg_ssid_t        *ptr;

    ptr = (acfg_ssid_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_os_send_req(vap_name, &req);

    if (status == QDF_STATUS_SUCCESS)
        ssid->len = acfg_os_strcpy((uint8_t *)ssid->ssid, (uint8_t *)ptr->ssid, ACFG_MAX_SSID_LEN + 1);

    return status;
}

/**
 * @brief Get the RSSI
 *
 * @param vap_name
 * @param rssi
 */
uint32_t
acfg_get_rssi(uint8_t  *vap_name, acfg_rssi_t  *rssi)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RSSI};
    acfg_rssi_t        *ptr;

    ptr = (acfg_rssi_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_os_send_req(vap_name, &req);

    if (status == QDF_STATUS_SUCCESS)
        memcpy(rssi, ptr, sizeof(acfg_rssi_t));

    return status;
}

/**
 * @brief Set the channel numbers
 *
 * @param wifi_name (Radio interface)
 * @param chan_num (IEEE Channel number)
 *
 * @return
 */
uint32_t
acfg_set_channel(uint8_t  *wifi_name, uint8_t  chan_num)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_CHANNEL};
    acfg_chan_t        *ptr;

    ptr = (acfg_chan_t *)req.data;

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    *ptr = chan_num;

    status = acfg_os_send_req(wifi_name, &req);

    return status;

}


/**
 * @brief Get the channel number
 *
 * @param wifi_name (Radio interface)
 * @param chan_num
 *
 * @return
 */
uint32_t
acfg_get_channel(uint8_t *wifi_name, uint8_t *chan_num)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_CHANNEL};

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_os_send_req(wifi_name, &req);

    if(status == QDF_STATUS_SUCCESS)
        *chan_num = *(acfg_chan_t *)req.data;

    return status;
}


/**
 * @brief Set the opmode
 *
 * @param vap_name (VAP interface)
 * @param opmode
 *
 * @return
 */
uint32_t
acfg_set_opmode(uint8_t *vap_name, acfg_opmode_t opmode)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_opmode_t * p_opmode = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_OPMODE};

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    p_opmode = (acfg_opmode_t *)req.data;
    *p_opmode = opmode;

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

acfg_opmode_t
acfg_convert_opmode(uint32_t opmode)
{
    switch(opmode) {
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
}

/**
 * @brief Get the opmode
 *
 * @param vap_name (VAP interface)
 * @param opmode
 *
 * @return
 */
uint32_t
acfg_get_opmode(uint8_t *vap_name, acfg_opmode_t *opmode)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_OPMODE};

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_os_send_req(vap_name, &req);

    if(status == QDF_STATUS_SUCCESS)
    {
        *opmode = acfg_convert_opmode((acfg_opmode_t)req.data);
        if(*opmode == (acfg_opmode_t)-1) {
            acfg_log_errstr("%s: Failed to convert opmode (vap=%s, opmode=%d)\n",
                    __func__,
                    vap_name,
                    (acfg_opmode_t)req.data);
            status = QDF_STATUS_E_FAILURE;
        }
    }

    return status ;
}

/**
 * @brief Set the frequency
 *
 * @param wifi_name
 * @param freq - Frequency in MHz
 *
 * @return
 */
uint32_t
acfg_set_freq(uint8_t *wifi_name, uint32_t freq)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    uint32_t *     p_freq = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_FREQUENCY};

    p_freq = (uint32_t *)req.data;
    *p_freq = freq;

    status = acfg_os_send_req(wifi_name, &req);
    return status ;
}

/**
 * @brief Set RTS threshold
 *
 * @param vap_name
 * @param rts value
 * @param rts flags
 * @return
 */
uint32_t
acfg_set_rts(uint8_t *vap_name, acfg_rts_t *rts)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_rts_t *   p_rts = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_RTS};

    p_rts = (acfg_rts_t *)req.data;
    *p_rts = *rts;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/**
 * @brief Get RTS threshold
 *
 * @param vap_name
 * @param rts
 *
 * @return
 */
uint32_t
acfg_get_rts(uint8_t *vap_name, acfg_rts_t *rts)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_rts_t * p_rts = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RTS};

    status = acfg_os_send_req(vap_name, &req);

    p_rts = (acfg_rts_t *)req.data;
    *rts = *p_rts;

    return status ;
}

/**
 * @brief Set frag threshold
 *
 * @param vap_name
 * @param frag
 *
 * @return
 */
uint32_t
acfg_set_frag(uint8_t *vap_name, acfg_frag_t *frag)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_frag_t *  p_frag = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_FRAG};

    p_frag = (acfg_frag_t *)req.data;
    *p_frag = *frag;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/**
 * @brief Get Fragmentation threshold
 *
 * @param vap_name
 * @param frag
 *
 * @return
 */
uint32_t
acfg_get_frag(uint8_t *vap_name, acfg_frag_t *frag)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_frag_t *  p_frag = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_FRAG};

    status = acfg_os_send_req(vap_name, &req);

    p_frag = (acfg_frag_t *)req.data;
    *frag = *p_frag;

    return status ;
}


/**
 * @brief Set txpower
 *
 * @param vap_name
 * @param txpower
 * @param flags
 * @return
 */
uint32_t
acfg_set_txpow(uint8_t *vap_name, acfg_txpow_t *txpow)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_txpow_t * p_txpow = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_TXPOW};

    p_txpow = (acfg_txpow_t *)req.data;
    *p_txpow = *txpow;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/**
 * @brief Get default Tx Power in dBm
 *
 * @param wifi_name
 * @param iwparam
 *
 * @return
 */
uint32_t
acfg_get_txpow(uint8_t *wifi_name, acfg_txpow_t *txpow)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_txpow_t * p_txpow = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_TXPOW};

    status = acfg_os_send_req(wifi_name, &req);

    p_txpow = (acfg_txpow_t *)req.data;
    *txpow = *p_txpow;

    return status ;
}


/**
 * @brief Get Access Point Mac Address
 *
 * @param vap_name
 * @param iwparam
 *
 * @return
 */
uint32_t
acfg_get_ap(uint8_t *vap_name, acfg_macaddr_t *mac)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_AP};
    acfg_macaddr_t *ptr;

    ptr = (acfg_macaddr_t *)req.data;

    status = acfg_os_send_req(vap_name, &req);

    acfg_os_strcpy(mac->addr, ptr->addr , ACFG_MACADDR_LEN) ;

    return status ;
}


/**
 * @brief Set the encode
 *
 * @param wifi_name
 * @param enc - encode string
 *
 * @return
 */
uint32_t
acfg_set_enc(uint8_t *wifi_name, acfg_encode_flags_t flag, char *enc)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_ENCODE};
    acfg_encode_t      *ptr;
    const char *p;
    int dlen;
    unsigned char out[ACFG_ENCODING_TOKEN_MAX] = {0};
    unsigned char key[ACFG_ENCODING_TOKEN_MAX];
    int keylen = 0;

    ptr = (acfg_encode_t *)req.data;

    p = (const char *)enc;
    dlen = -1;

    if(!(flag & ACFG_ENCODE_DISABLED) && (enc != NULL)) {
        while(*p != '\0') {
            unsigned int temph;
            unsigned int templ;
            int count;
            if(dlen <= 0) {
                if(dlen == 0)
                    p++;
                dlen = strcspn(p, "-:;.,");
            }
            count = sscanf(p, "%1X%1X", &temph, &templ);
            if(count < 1)
                return -1;
            if(dlen % 2)
                count = 1;
            if(count == 2)
                templ |= temph << 4;
            else
                templ = temph;
            out[keylen++] = (unsigned char) (templ & 0xFF);

            if(keylen >= ACFG_ENCODING_TOKEN_MAX )
                break;

            p += count;
            dlen -= count;
        }

        memcpy(key, out, keylen);
        ptr->buff = key;
        ptr->len = keylen;
    }
    else {
        ptr->buff = NULL;
        ptr->len = 0;
    }
    ptr->flags = flag;

    if(ptr->buff == NULL)
        ptr->flags |= ACFG_ENCODE_NOKEY;

    status = acfg_os_send_req(wifi_name, &req);

    return status;
}


/**
 * @brief Set Vap param
 *
 * @param vap_name
 * @param param
 * @param val
 *
 * @return
 */
uint32_t
acfg_set_vap_param(uint8_t *vap_name, \
        acfg_param_vap_t param, uint32_t val)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_VAP_PARAM};
    acfg_param_req_t *ptr;
    ptr = (acfg_param_req_t *)req.data;

    ptr->param = param ;
    ptr->val = val ;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}


/**
 * @brief Get Vap param
 *
 * @param vap_name
 * @param param
 * @param val
 *
 * @return
 */
uint32_t
acfg_get_vap_param(uint8_t *vap_name, \
        acfg_param_vap_t param, uint32_t *val)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_VAP_PARAM};
    acfg_param_req_t *ptr;

    ptr = (acfg_param_req_t *)req.data;
    ptr->param = param ;

    status = acfg_os_send_req(vap_name, &req);

    *val = ptr->val ;

    return status ;
}


/**
 * @brief set Vap vendor param
 *
 * @param vap_name
 * @param param
 * @param data
 * @param len
 *
 * @return
 */
uint32_t
acfg_set_vap_vendor_param(uint8_t *vap_name, \
        acfg_vendor_param_vap_t param, uint8_t *data,
        uint32_t len, uint32_t type, acfg_vendor_param_init_flag_t reinit)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_VAP_VENDOR_PARAM};
    acfg_vendor_param_req_t *ptr;

    ptr = (acfg_vendor_param_req_t *)req.data;
    ptr->param = param ;
    ptr->type = type;

    if(len <= sizeof(acfg_vendor_param_data_t))
        memcpy(&ptr->data, data, len);
    else
    {
        acfg_log_errstr("Vendor param size greater than max allowed by ACFG!\n");
        return status;
    }

    status = acfg_os_send_req(vap_name, &req);

    if(reinit == RESTART_SECURITY && status == QDF_STATUS_SUCCESS)
    {
        acfg_opmode_t opmode;
        char cmd[15], replybuf[255];
        uint32_t len;

        memset(replybuf, 0, sizeof(replybuf));
        status = acfg_get_opmode(vap_name, &opmode);
        if(status != QDF_STATUS_SUCCESS){
            return status;
        }
        acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
                ctrl_wpasupp);
        if(opmode == ACFG_OPMODE_HOSTAP)
            acfg_os_strcpy((uint8_t *)cmd, (uint8_t *)"RELOAD", sizeof(cmd));
        else
            acfg_os_strcpy((uint8_t *)cmd, (uint8_t *)"RECONNECT", sizeof(cmd));
        /* reload the security */
        if((acfg_ctrl_req (vap_name,
                        cmd,
                        strlen(cmd),
                        replybuf, &len,
                        opmode) < 0) ||
                strncmp(replybuf, "OK", strlen("OK"))){
            acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                    cmd,
                    vap_name);
            return QDF_STATUS_E_FAILURE;
        }
    }

    return status ;
}


/**
 * @brief Get Vap vendor param
 *
 * @param vap_name
 * @param param
 * @param val
 * @param type
 *
 * @return
 */
uint32_t
acfg_get_vap_vendor_param(uint8_t *vap_name, \
        acfg_param_vap_t param, uint8_t *data, uint32_t *type)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_VAP_VENDOR_PARAM};
    acfg_vendor_param_req_t *ptr;

    ptr = (acfg_vendor_param_req_t *)req.data;
    ptr->param = param ;

    status = acfg_os_send_req(vap_name, &req);

    if(status == QDF_STATUS_SUCCESS){
        memcpy(data, &ptr->data, sizeof(acfg_vendor_param_data_t));
        *type = ptr->type;
    }

    return status ;
}


/**
 * @brief Set Radio param
 *
 * @param radio_name
 * @param param
 * @param val
 *
 * @return
 */
uint32_t
acfg_set_radio_param(uint8_t *radio_name, \
        acfg_param_radio_t param, uint32_t val)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_RADIO_PARAM};
    acfg_param_req_t *ptr;
    ptr = (acfg_param_req_t *)req.data;

    ptr->param = param ;
    ptr->val = val ;

    if(acfg_os_cmp_str(radio_name,(uint8_t *)"wifi",4)){
        acfg_log_errstr("Should use wifiX to set radio param.\n");
        return status ;
    }

    status = acfg_os_send_req(radio_name, &req);

    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: failed (param=0x%x status=%d)\n", __func__, param, status);
    }
    return status ;
}


/**
 * @brief Get Radio param
 *
 * @param radio_name
 * @param param
 * @param val
 *
 * @return
 */
uint32_t
acfg_get_radio_param(uint8_t *radio_name, \
        acfg_param_radio_t param, uint32_t *val)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RADIO_PARAM};
    acfg_param_req_t *ptr;

    ptr = (acfg_param_req_t *)req.data;
    ptr->param = param;

    if(acfg_os_cmp_str(radio_name,(uint8_t *)"wifi",4)){
        acfg_log_errstr("Should use wifiX to get radio param.\n");
        return status ;
    }

    status = acfg_os_send_req(radio_name, &req);
    ptr = (acfg_param_req_t *)req.data;

    *val = ptr->val;

    return status ;
}

/**
 * @Set bit rate
 *
 * @param vap_name
 * @param rate val
 * @param rate fixed
 * @return
 */
uint32_t
acfg_set_rate(uint8_t *vap_name, acfg_rate_t *rate)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_rate_t *  p_rate = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_RATE};

    p_rate = (acfg_rate_t *)req.data;
    *p_rate = *rate;

    status = acfg_os_send_req(vap_name, &req);

    return status ;
}

/**
 * @brief Get default bit rate
 *
 * @param vap_name
 * @param rate
 *
 * @return
 */
uint32_t
acfg_get_rate(uint8_t *vap_name, uint32_t *rate)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    uint32_t *     p_rate = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_RATE};

    status = acfg_os_send_req(vap_name, &req);

    /* Driver returns bitrate in Kbps
     * so coverting back to Bps
     */
    p_rate = (uint32_t *)req.data;
    *rate = *p_rate * 1000;

    return status ;
}

/**
 * @brief Set the phymode
 *
 * @param vap_name
 * @param mode
 *
 * @return
 */
uint32_t
acfg_set_phymode(uint8_t *vap_name, acfg_phymode_t mode)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SET_PHYMODE};
    acfg_phymode_t *ptr;

    ptr = (acfg_phymode_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    *ptr = mode ;
    status = acfg_os_send_req(vap_name, &req);

    return status;
}

/**
 * @brief Get the phymode
 *
 * @param vap_name
 * @param mode
 *
 * @return
 */
uint32_t
acfg_get_phymode(uint8_t *vap_name, acfg_phymode_t *mode)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    acfg_phymode_t * p_mode = NULL;
    acfg_os_req_t req = {.cmd = ACFG_REQ_GET_PHYMODE};

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_os_send_req(vap_name, &req);

    if (status == QDF_STATUS_SUCCESS) {
        p_mode = (acfg_phymode_t *)req.data;
        *mode = *p_mode;
    }

    return status;
}

/**
 * @brief
 *
 * @param vap_name
 * @param sinfo
 *
 * @return
 */
uint32_t
acfg_assoc_sta_info(uint8_t *vap_name, acfg_sta_info_req_t *sinfo)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t req = {.cmd = ACFG_REQ_GET_ASSOC_STA_INFO};
    acfg_sta_info_req_t *ptr ;

    ptr = (acfg_sta_info_req_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    ptr->len = sinfo->len ;
    ptr->info = sinfo->info ;

    status = acfg_os_send_req(vap_name, &req);

    sinfo->len = ptr->len ;

    return status;
}

uint32_t
acfg_mon_enable_filter(uint8_t *vap_name, uint32_t val)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    uint32_t *     p_val = NULL;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_MON_ENABLE_FILTER};

    p_val = (uint32_t *)req.data;
    *p_val = val;
    status = acfg_os_send_req(vap_name, &req);

    return status;
}


uint32_t
acfg_mon_listmac(uint8_t *vap_name)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_MON_LISTMAC};

    status = acfg_os_send_req(vap_name, &req);

    return status;
}


/**
 * @brief mon addmac
 *
 * @param vap name
 * @param mac addr
 *
 *
 * @return
 */
uint32_t
acfg_mon_addmac(uint8_t *vap_name, uint8_t *addr)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_MON_ADDMAC};
    acfg_macaddr_t *mac;

    mac = (acfg_macaddr_t *)req.data;

    memcpy(mac->addr, addr, ACFG_MACADDR_LEN);
    status = acfg_os_send_req(vap_name, &req);

    return status;
}

/**
 * @brief mon delmac
 *
 * @param vap name
 * @param mac addr
 *
 *
 * @return
 */
uint32_t
acfg_mon_delmac(uint8_t *vap_name, uint8_t *addr)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_MON_DELMAC};
    acfg_macaddr_t *mac;

    mac = (acfg_macaddr_t *)req.data;

    memcpy(mac->addr, addr, ACFG_MACADDR_LEN);
    status = acfg_os_send_req(vap_name, &req);

    return status;
}

/**
 * @brief acl addmac
 *
 * @param vap name
 * @param mac addr
 *
 *
 * @return
 */
uint32_t
acfg_acl_addmac(uint8_t *vap_name, uint8_t *addr)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_ACL_ADDMAC};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    //acfg_str_to_ether(addr, &sa);
    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);

    mac = (acfg_macaddr_t *)req.data;

    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;
}


/**
 * @brief acl getmac
 *
 * @param vap_name
 *
 *
 *
 * @return
 */
uint32_t
acfg_acl_getmac(uint8_t *vap_name, acfg_macacl_t *maclist)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_MAC_ADDR};
    acfg_macacl_t *list;
    uint32_t i = 0;

    list = (acfg_macacl_t *)req.data;

    status = acfg_os_send_req(vap_name, &req);
    if(status == QDF_STATUS_SUCCESS){
        for (i = 0; i < list->num; i++) {
            memcpy(maclist->macaddr[i], list->macaddr[i], ACFG_MACADDR_LEN);
        }
        maclist->num = list->num;
    }

    return status;
}

/**
 * @brief acl delmac
 *
 * @param vap_name
 * @param macaddr
 * @
 *
 * @return
 */
uint32_t
acfg_acl_delmac(uint8_t *vap_name, uint8_t *addr)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_ACL_DELMAC};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);
    mac = (acfg_macaddr_t *)req.data;
    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;

}

/* Secondary ACL list implementation */
/**
 * @brief acl addmac_secondary
 *
 * @param vap name
 * @param mac addr
 *
 *
 * @return
 */
uint32_t
acfg_acl_addmac_secondary(uint8_t *vap_name, uint8_t *addr)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_ACL_ADDMAC_SEC};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);

    mac = (acfg_macaddr_t *)req.data;

    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;
}



/**
 * @brief acl getmac_secondary
 *
 * @param vap_name
 *
 *
 *
 * @return
 */
uint32_t
acfg_acl_getmac_secondary(uint8_t *vap_name, acfg_macacl_t *maclist)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_ACL_GETMAC_SEC};
    acfg_macacl_t *list;
    uint32_t i = 0;

    list = (acfg_macacl_t *)req.data;

    status = acfg_os_send_req(vap_name, &req);
    if(status == QDF_STATUS_SUCCESS){
        for (i = 0; i < list->num; i++) {
            memcpy(maclist->macaddr[i], list->macaddr[i], ACFG_MACADDR_LEN);
        }
        maclist->num = list->num;
    }

#if 0
    memcpy(maclist, macacllist,
            (sizeof(macacllist->num) + macacllist->num * ACFG_MACADDR_LEN) );
#endif

    return status;
}

/**
 * @brief acl delmac_secondary
 *
 * @param vap_name
 * @param macaddr
 *
 * @return
 */
uint32_t
acfg_acl_delmac_secondary(uint8_t *vap_name, uint8_t *addr)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_ACL_DELMAC_SEC};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);
    mac = (acfg_macaddr_t *)req.data;
    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;

}

uint32_t
acfg_set_ap(uint8_t *vap_name, uint8_t *addr)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_AP};
    acfg_macaddr_t *mac;
    struct sockaddr sa;

    memcpy(sa.sa_data, addr, ACFG_MACADDR_LEN);

    mac = (acfg_macaddr_t *)req.data;

    memcpy(mac->addr, sa.sa_data, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;
}


uint32_t
acfg_wlan_iface_present(char *ifname)
{
    struct ifreq ifr;
    int s;
    uint32_t   status = QDF_STATUS_SUCCESS;

    memset(&ifr, 0, sizeof(struct ifreq));
    strlcpy(ifr.ifr_name, (char *)ifname, ACFG_MAX_IFNAME);

    ifr.ifr_data = (__caddr_t)NULL;

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = QDF_STATUS_E_BUSY;
        acfg_log_errstr("Unable to open the socket\n");
        goto fail;
    }

    if (ioctl (s, SIOCGIFFLAGS, &ifr) < 0) {
        acfg_log_errstr("Interface %s Not Present\n", ifname);
        status = acfg_get_err_status();
        //acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
    }

    close(s);

fail:
    return status;
}

static int
acfg_str_to_ether(char *bufp, struct sockaddr *sap)
{
#define ETH_ALEN    6
    unsigned char *ptr;
    int i, j;
    unsigned char val;
    unsigned char c;

    ptr = (unsigned char *) sap->sa_data;

    i = 0;

    do {
        j = val = 0;

        /* We might get a semicolon here - not required. */
        if (i && (*bufp == ':')) {
            bufp++;
        }

        do {
            c = *bufp;
            if (((unsigned char)(c - '0')) <= 9) {
                c -= '0';
            } else if (((unsigned char)((c|0x20) - 'a')) <= 5) {
                c = (c|0x20) - ('a'-10);
            } else if (j && (c == ':' || c == 0)) {
                break;
            } else {
                return -1;
            }
            ++bufp;
            val <<= 4;
            val += c;
        } while (++j < 2);
        *ptr++ = val;
    } while (++i < ETH_ALEN);
    return (int) (*bufp);   /* Error if we don't end at end of string. */
#undef ETH_ALEN
}

void
acfg_mac_str_to_octet(uint8_t *mac_str, uint8_t *mac)
{
    char val[3], *str, *str1;
    int i = 0;
    if((str1 = strtok((char *)mac_str, ":")) != NULL){
        acfg_os_strcpy((uint8_t *)val,(uint8_t *)str1,sizeof(val));
	mac[i] = (uint8_t)strtol(val, NULL, 16);
	i++;
	while (((str = strtok(0, ":")) != NULL) && (i < ACFG_MACADDR_LEN)) {
	   acfg_os_strcpy((uint8_t *)val, (uint8_t *)str, sizeof(val));
	    mac[i] = (uint8_t)strtol(val, NULL, 16);
	    i++;
        }
    }
}


uint32_t
acfg_set_ifmac (char *ifname, char *buf, int arphdr)
{
    struct sockaddr sa;
    uint32_t   status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_HW_ADDR};
    acfg_macaddr_t  *ptr = NULL;
    u_int8_t *wifi_name = (u_int8_t *) ifname;

    if(!ifname) {
        return status;
    }

    if (acfg_str_to_ether(buf, &sa) == 0) {
        sa.sa_family = arphdr;
        ptr = (acfg_macaddr_t *)req.data;

        if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
            return QDF_STATUS_E_NOENT;

        memcpy(&ptr->addr, &sa.sa_data, ACFG_MACADDR_LEN);
        status = acfg_os_send_req(wifi_name, &req);
    }

    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
    }

    return status;
}

uint32_t
acfg_get_ifmac (char *ifname, char *buf)
{
    struct ifreq ifr;
    uint32_t   status = QDF_STATUS_SUCCESS;
    int s;
    int i = 0;
    uint8_t *ptr;

    memset(&ifr, 0, sizeof(struct ifreq));

    if(!ifname) {
        return QDF_STATUS_E_FAILURE;
    }

    acfg_os_strcpy((uint8_t *)ifr.ifr_name, (uint8_t *)ifname, ACFG_MAX_IFNAME);

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = QDF_STATUS_E_BUSY;
        goto fail;
    }

    if ((i = ioctl (s, SIOCGIFHWADDR, &ifr)) < 0) {
        status = acfg_get_err_status();
        acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
        close(s);
        goto fail;
    }

    ptr = (uint8_t *) ifr.ifr_hwaddr.sa_data;
    snprintf(buf, ACFG_MACSTR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",
            (ptr[0] & 0377), (ptr[1] & 0377), (ptr[2] & 0377),
            (ptr[3] & 0377), (ptr[4] & 0377), (ptr[5] & 0377));

    close(s);

fail:
    return status;
}

uint32_t
acfg_wlan_profile_get(acfg_wlan_profile_t *profile)
{
    int i;
    uint32_t status = QDF_STATUS_SUCCESS;

    status = acfg_wlan_iface_present((char *)profile->radio_params.radio_name);
    if(status != QDF_STATUS_SUCCESS) {
        return QDF_STATUS_E_INVAL;
    }

    status = acfg_get_current_profile(profile);
    if(status != QDF_STATUS_SUCCESS){
        acfg_log_errstr("%s: Failed to get driver profile for one or more vaps\n",
                __func__);
        return status;
    }

    for (i = 0; i < profile->num_vaps; i++) {
        if (profile->vap_params[i].opmode == IEEE80211_M_STA) {
            status = acfg_wpa_supplicant_get(&(profile->vap_params[i]));
            if(status != QDF_STATUS_SUCCESS)
            {
                acfg_log_errstr("%s: Failed to get security profile for %s\n",
                        __func__,
                        profile->vap_params[i].vap_name);
                return status;
            }
        }
        if (profile->vap_params[i].opmode == ACFG_OPMODE_HOSTAP) {
            status = acfg_hostapd_get(&(profile->vap_params[i]));
            if(status != QDF_STATUS_SUCCESS)
            {
                acfg_log_errstr("%s: Failed to get security profile for %s\n",
                        __func__,
                        profile->vap_params[i].vap_name);
                return status;
            }
        }
    }

    return QDF_STATUS_SUCCESS;
}

uint32_t
acfg_hostapd_getconfig(uint8_t *vap_name, char *reply_buf)
{
    uint32_t       status = QDF_STATUS_SUCCESS;
    char buffer[4096];
    uint32_t len = 0;

    acfg_os_strcpy((uint8_t *)buffer, (uint8_t *)"GET_CONFIG", sizeof(buffer));

    len = sizeof (reply_buf);
    if(acfg_ctrl_req (vap_name, buffer, strlen(buffer),
                reply_buf, &len, ACFG_OPMODE_HOSTAP) < 0){
        status = QDF_STATUS_E_FAILURE;
    }

    return status;
}

uint32_t
acfg_wlan_vap_profile_get (acfg_wlan_profile_vap_params_t *vap_params)
{
    (void)vap_params;
    uint32_t status = QDF_STATUS_SUCCESS;

    return status;
}

static uint32_t
acfg_wlan_app_iface_up(acfg_wlan_profile_vap_params_t *vap_params)
{
    int no_auth;
    char cmd[512];
    char reply[255];
    uint32_t len = sizeof (reply);

    if (vap_params == NULL) {
        return QDF_STATUS_E_FAILURE;
    }

    memset(reply, 0, sizeof(reply));
    no_auth = ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
               (vap_params->security_params.cipher_method ==
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP));

    if (!no_auth) {
        /* Enable the network at hostapd or supplicant level first */
        if (vap_params->opmode == ACFG_OPMODE_STA) {
            /* For supplicant */
            memset(cmd, '\0', sizeof (cmd));
            snprintf(cmd, sizeof(cmd), "%s %d", WPA_ENABLE_NETWORK_CMD_PREFIX, 0);
            if((acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd), reply,
                              &len, ACFG_OPMODE_STA) < 0) ||
                              strncmp (reply, "OK", strlen("OK"))){
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                                cmd, vap_params->vap_name);
                return QDF_STATUS_E_FAILURE;
            }
        } else {
            /* For hostapd */
            memset(cmd, '\0', sizeof (cmd));
            snprintf(cmd, sizeof(cmd), "%s", "STATUS");
            if(acfg_ctrl_req (vap_params->vap_name, cmd, strlen(cmd), reply, &len,
                              vap_params->opmode) < 0) {
                acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                                cmd, vap_params->vap_name);
                return QDF_STATUS_E_FAILURE;
            }
            if(!strncmp (reply, "state=ENABLED", strlen("state=ENABLED"))){
                /* Already Enabled, nothing to do */
                return QDF_STATUS_SUCCESS;
            } else {
                memset(cmd, '\0', sizeof (cmd));
                snprintf(cmd, sizeof(cmd), "%s", "ENABLE");
                if((acfg_ctrl_req (vap_params->vap_name, cmd, strlen(cmd), reply,
                                   &len, vap_params->opmode) < 0) ||
                                   strncmp (reply, "OK", strlen("OK"))){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                                    cmd, vap_params->vap_name);
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
    }

    return QDF_STATUS_SUCCESS;
}

uint32_t
acfg_wlan_iface_up(uint8_t  *ifname, acfg_wlan_profile_vap_params_t *vap_params)
{
    struct ifreq ifr;
    int s;
    uint32_t   status = QDF_STATUS_SUCCESS;
    int no_auth;

    /* Check if hostapd/supplicant iface needs to be brought up */
    if (vap_params != NULL) {
        if (acfg_wlan_app_iface_up(vap_params) != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }

        no_auth = ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
               (vap_params->security_params.cipher_method ==
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP));

        if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) && !no_auth) {
            /* Hostapd also brings up the VAP, so return here */
            return QDF_STATUS_SUCCESS;
        }
    }

    memset(&ifr, 0, sizeof(struct ifreq));

    if(!ifname)
        return QDF_STATUS_E_FAILURE;

    strlcpy(ifr.ifr_name, (char *)ifname, ACFG_MAX_IFNAME);

    ifr.ifr_data = (__caddr_t)NULL;
    ifr.ifr_flags = (IFF_UP | IFF_RUNNING);

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = QDF_STATUS_E_BUSY;
        goto fail;
    }

    if (ioctl (s, SIOCSIFFLAGS, &ifr) < 0) {
        status = acfg_get_err_status();
        acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
    }

    close(s);

fail:
    return status;
}

static uint32_t
acfg_wlan_app_iface_down(acfg_wlan_profile_vap_params_t *vap_params)
{
    int no_auth;
    char cmd[512];
    char reply[255];
    uint32_t len = sizeof (reply);
    uint32_t   status = QDF_STATUS_SUCCESS;

    if (vap_params == NULL) {
        return QDF_STATUS_E_FAILURE;
    }

    memset(reply, 0, sizeof(reply));
    no_auth = ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
               (vap_params->security_params.cipher_method ==
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP));

    if (!no_auth) {
        /* Disable the network at hostapd or supplicant level first */
        if (vap_params->opmode == ACFG_OPMODE_STA) {
            /* For supplicant */
            memset(cmd, '\0', sizeof (cmd));
            snprintf(cmd, sizeof(cmd), "%s", "STATUS");
            status = acfg_ctrl_req (vap_params->vap_name, cmd, strlen(cmd), reply,
                                    &len, vap_params->opmode);
            if((status > 0)) {
                /* No iface at supplicant */
                return QDF_STATUS_SUCCESS;
            } else {
                memset(cmd, '\0', sizeof (cmd));
                snprintf(cmd, sizeof(cmd), "%s %d", WPA_DISABLE_NETWORK_CMD_PREFIX, 0);
                if((acfg_ctrl_req(vap_params->vap_name, cmd, strlen(cmd), reply,
                                  &len, ACFG_OPMODE_STA) < 0) ||
                                  strncmp (reply, "OK", strlen("OK"))){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                                    cmd, vap_params->vap_name);
                    return QDF_STATUS_E_FAILURE;
                }
            }
        } else {
            /* For hostapd */
            memset(cmd, '\0', sizeof (cmd));
            snprintf(cmd, sizeof(cmd), "%s", "STATUS");
            status = acfg_ctrl_req (vap_params->vap_name, cmd, strlen(cmd), reply,
                                    &len, vap_params->opmode);
            if((status > 0) ||
               !strncmp (reply, "state=DISABLED", strlen("state=DISABLED")) ||
               !strncmp (reply, "state=UNINITIALIZED", strlen("state=UNINITIALIZED"))){
                /* No iface at hostapd or Already Disabled or un-initialized,
                 * nothing to do */
                return QDF_STATUS_SUCCESS;
            } else {
                memset(cmd, '\0', sizeof (cmd));
                snprintf(cmd, sizeof(cmd), "%s", "DISABLE");
                if((acfg_ctrl_req (vap_params->vap_name, cmd, strlen(cmd), reply,
                                   &len, vap_params->opmode) < 0) ||
                                   strncmp (reply, "OK", strlen("OK"))){
                    acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                                    cmd, vap_params->vap_name);
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
    }

    return QDF_STATUS_SUCCESS;
}

uint32_t
acfg_wlan_iface_down(uint8_t *ifname, acfg_wlan_profile_vap_params_t *vap_params)
{
    struct ifreq ifr;
    int s;
    uint32_t   status = QDF_STATUS_SUCCESS;
    int no_auth;

    /* Check if hostapd/supplicant iface needs to be brought down */
    if (vap_params != NULL) {
        if (acfg_wlan_app_iface_down(vap_params) != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
        no_auth = ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
               (vap_params->security_params.cipher_method ==
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP));

        if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) && !no_auth) {
            /* Hostapd also brings down the VAP, so return here */
            return QDF_STATUS_SUCCESS;
        }
    }

    memset(&ifr, 0, sizeof(struct ifreq));

    if(!ifname)
        return -QDF_STATUS_E_FAILURE;

    strlcpy(ifr.ifr_name, (char *)ifname, ACFG_MAX_IFNAME);
    ifr.ifr_data = (__caddr_t)NULL;
    ifr.ifr_flags = 0;

    s = socket(AF_INET, SOCK_DGRAM, 0);

    if(s < 0) {
        status = QDF_STATUS_E_BUSY;
        goto fail;
    }

    if (ioctl (s, SIOCSIFFLAGS, &ifr) < 0) {
        status = acfg_get_err_status();
        acfg_log_errstr("%s: IOCTL failed (status=%d)\n", __func__, status);
    }

    close(s);

fail:
    return status;
}

uint32_t
acfg_set_acl_policy(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    acfg_wlan_profile_node_params_t node_params, cur_node_params;
    uint32_t status = QDF_STATUS_SUCCESS;

    node_params = vap_params->node_params;
    if (cur_vap_params != NULL) {
        cur_node_params = cur_vap_params->node_params;
        if (node_params.node_acl != cur_node_params.node_acl) {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_MACCMD,
                    node_params.node_acl);
            if (status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
    } else {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_MACCMD,
                node_params.node_acl);
    }
    return status;
}

uint32_t
acfg_set_acl_policy_secondary(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    acfg_wlan_profile_node_params_t node_params, cur_node_params;
    uint32_t status = QDF_STATUS_SUCCESS;
    node_params = vap_params->node_params;
    if (cur_vap_params != NULL) {
        cur_node_params = cur_vap_params->node_params;
        if (node_params.node_acl_sec != cur_node_params.node_acl_sec) {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_MACCMD_SEC,
                    node_params.node_acl_sec);
            if (status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
    } else {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_MACCMD_SEC,
                node_params.node_acl_sec);
    }
    return status;
}

uint32_t
acfg_set_node_list_secondary(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    acfg_wlan_profile_node_params_t node_params, cur_node_params;
    uint8_t *mac;
    uint8_t new_index, cur_index, found ;
    uint32_t status = QDF_STATUS_SUCCESS;

    node_params = vap_params->node_params;
    if (cur_vap_params != NULL) {
        cur_node_params = cur_vap_params->node_params;
        for (new_index = 0; new_index < node_params.num_node_sec; new_index++) {
            mac = node_params.acfg_acl_node_list_sec[new_index];
            found = 0;
            for (cur_index = 0; cur_index < cur_node_params.num_node_sec;
                    cur_index++)
            {
                if (memcmp(mac,
                            cur_node_params.acfg_acl_node_list_sec[cur_index],
                            ACFG_MACADDR_LEN) == 0)
                {
                    found = 1;
                    break;
                }

            }
            if (found == 0) {
                status = acfg_acl_addmac_secondary((uint8_t *)vap_params->vap_name,
                        mac);
                if(status != QDF_STATUS_SUCCESS) {
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
        for (cur_index = 0; cur_index < cur_node_params.num_node_sec;
                cur_index++)
        {
            mac = cur_node_params.acfg_acl_node_list_sec[cur_index];
            found = 0;
            for (new_index = 0; new_index < node_params.num_node_sec;
                    new_index++)
            {
                if (memcmp(mac,
                            node_params.acfg_acl_node_list_sec[new_index],
                            ACFG_MACADDR_LEN) == 0)
                {
                    found = 1;
                    break;
                }

            }
            if (found == 0) {
                status = acfg_acl_delmac_secondary((uint8_t *)cur_vap_params->vap_name,
                        mac);
                if(status != QDF_STATUS_SUCCESS) {
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
    } else {
        for (new_index = 0; new_index < node_params.num_node_sec; new_index++) {
            mac = node_params.acfg_acl_node_list_sec[new_index];
            status = acfg_acl_addmac_secondary((uint8_t *)vap_params->vap_name,
                    mac);
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
    return status;
}


uint32_t
acfg_set_node_list(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    acfg_wlan_profile_node_params_t node_params, cur_node_params;
    uint8_t *mac;
    uint8_t new_index, cur_index, found ;
    uint32_t status = QDF_STATUS_SUCCESS;

    node_params = vap_params->node_params;
    if (cur_vap_params != NULL) {
        cur_node_params = cur_vap_params->node_params;
        for (new_index = 0; new_index < node_params.num_node; new_index++) {
            mac = node_params.acfg_acl_node_list[new_index];
            found = 0;
            for (cur_index = 0; cur_index < cur_node_params.num_node;
                    cur_index++)
            {
                if (memcmp(mac,
                            cur_node_params.acfg_acl_node_list[cur_index],
                            ACFG_MACADDR_LEN) == 0)
                {
                    found = 1;
                    break;
                }

            }
            if (found == 0) {
                status = acfg_acl_addmac((uint8_t *)vap_params->vap_name,
                        mac);
                if(status != QDF_STATUS_SUCCESS) {
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
        for (cur_index = 0; cur_index < cur_node_params.num_node;
                cur_index++)
        {
            mac = cur_node_params.acfg_acl_node_list[cur_index];
            found = 0;
            for (new_index = 0; new_index < node_params.num_node;
                    new_index++)
            {
                if (memcmp(mac,
                            node_params.acfg_acl_node_list[new_index],
                            ACFG_MACADDR_LEN) == 0)
                {
                    found = 1;
                    break;
                }

            }
            if (found == 0) {
                status = acfg_acl_delmac((uint8_t *)cur_vap_params->vap_name,
                        mac);
                if(status != QDF_STATUS_SUCCESS) {
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
    } else {
        for (new_index = 0; new_index < node_params.num_node; new_index++) {
            mac = node_params.acfg_acl_node_list[new_index];
            status = acfg_acl_addmac((uint8_t *)vap_params->vap_name,
                    mac);
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
    return status;
}

void
acfg_rem_wps_config_file(uint8_t *ifname)
{
    char filename[32];
    FILE *fp;

    snprintf(filename, sizeof(filename), "/etc/%s_%s.conf",
             ACFG_WPS_CONFIG_PREFIX, (char *)ifname);
    fp = fopen(filename, "r");
    if (fp != NULL) {
        unlink(filename);
        fclose(fp);
    }
}

uint32_t
acfg_set_wps_vap_params( acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wps_cred_t *wps_cred)
{
    acfg_opmode_t opmode;
    uint32_t status = QDF_STATUS_SUCCESS;

    status = acfg_get_opmode(vap_params->vap_name, &opmode);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Opmode fetch fail for %s\n", __func__,
                vap_params->vap_name);
        return QDF_STATUS_E_FAILURE;
    }
    acfg_os_strcpy((uint8_t *)vap_params->ssid, (uint8_t *)wps_cred->ssid, sizeof(vap_params->ssid));
    if ( wps_cred->wpa == 1) {
        vap_params->security_params.sec_method =
            IEEE80211_AUTH_WPA;
    } else if ((wps_cred->wpa == 2)) {
        vap_params->security_params.sec_method =
            IEEE80211_AUTH_RSNA;
    } else if ((wps_cred->wpa == 3)) {
        vap_params->security_params.sec_method =
            ACFG_WLAN_PROFILE_SEC_METH_WPAWPA2;
    } else if (wps_cred->wpa == 0) {
        if (wps_cred->auth_alg == 1) {
            vap_params->security_params.sec_method =
                ACFG_WLAN_PROFILE_SEC_METH_OPEN;
        } else if (wps_cred->auth_alg == 2) {
            vap_params->security_params.sec_method =
                ACFG_WLAN_PROFILE_SEC_METH_SHARED;
        }
        if (strlen(wps_cred->wep_key)) {
            if (wps_cred->wep_key_idx == 0) {
                acfg_os_strcpy((uint8_t *)vap_params->security_params.wep_key0,
                               (uint8_t *)wps_cred->wep_key,
                               sizeof(vap_params->security_params.wep_key0));
            } else if (wps_cred->wep_key_idx == 1) {
                acfg_os_strcpy((uint8_t *)vap_params->security_params.wep_key1,
                               (uint8_t *)wps_cred->wep_key,
                               sizeof(vap_params->security_params.wep_key1));
            } else if (wps_cred->wep_key_idx == 2) {
                acfg_os_strcpy((uint8_t *)vap_params->security_params.wep_key2,
                               (uint8_t *)wps_cred->wep_key,
                               sizeof(vap_params->security_params.wep_key2));
            } else if (wps_cred->wep_key_idx == 3) {
                acfg_os_strcpy((uint8_t *)vap_params->security_params.wep_key3,
                               (uint8_t *)wps_cred->wep_key,
                               sizeof(vap_params->security_params.wep_key3));
            }
            vap_params->security_params.wep_key_defidx = wps_cred->wep_key_idx;
            vap_params->security_params.cipher_method =
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP;
        } else {
            vap_params->security_params.sec_method =
                ACFG_WLAN_PROFILE_SEC_METH_OPEN;
        }
    }

    if (wps_cred->key_mgmt == 2) {
        strlcpy(vap_params->security_params.psk, wps_cred->key, sizeof(vap_params->security_params.psk));
    }
    if (wps_cred->enc_type) {
        vap_params->security_params.cipher_method = wps_cred->enc_type;
    }
    /*Overide Cipher*/
    if ((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
            (vap_params->security_params.sec_method ==
             ACFG_WLAN_PROFILE_SEC_METH_SHARED))
    {
        if (strlen(wps_cred->wep_key)) {
            vap_params->security_params.cipher_method =
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP;
        } else {
            vap_params->security_params.cipher_method =
                ACFG_WLAN_PROFILE_CIPHER_METH_NONE;
        }
    }

    if (opmode == ACFG_OPMODE_HOSTAP) {
        vap_params->security_params.wps_flag = WPS_FLAG_CONFIGURED;
    }

    return status;
}

uint32_t
acfg_wps_config(uint8_t *ifname, char *ssid,
        char *auth, char *encr, char *key)
{
    char cmd[255];
    char buf[255];
    char replybuf[255];
    uint32_t len = sizeof(replybuf), i;
    acfg_opmode_t opmode;
    uint32_t status = QDF_STATUS_SUCCESS;
    char ssid_hex[2 * 32 + 1];
    char key_hex[2 * 64 + 1];

    memset(replybuf, 0, sizeof(replybuf));
    status = acfg_get_opmode(ifname,
            &opmode);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Opmode fetch fail for %s\n", __func__, ifname);
        return status;
    }
    acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
            ctrl_wpasupp);
    snprintf(cmd, sizeof(cmd), "WPS_CONFIG");
    if (strcmp(ssid, "0")) {
        ssid_hex[0] = '\0';
        for (i = 0; i < 32; i++) {
            if (ssid[i] == '\0') {
                break;
            }
            snprintf(&ssid_hex[i * 2], 3, "%02x", ssid[i]);
        }
        strlcat(cmd, " ", sizeof(cmd));
        strlcat(cmd, ssid_hex, sizeof(cmd));
    }
    if (strcmp(auth, "0")) {
        snprintf(buf, sizeof(buf), " %s", auth);
        strlcat(cmd, buf, sizeof(cmd));
    }
    if (strcmp(encr, "0")) {
        snprintf(buf, sizeof(buf), " %s", encr);
        strlcat(cmd, buf, sizeof(cmd));
    }
    if (strcmp(key, "0")) {
        key_hex[0] = '\0';
        for (i = 0; i < 64; i++) {
            if (key[i] == '\0') {
                break;
            }
            snprintf(&key_hex[i * 2], 3, "%02x",
                    key[i]);
        }
        strlcat(cmd, " ", sizeof(cmd));
        strlcat(cmd, key_hex, sizeof(cmd));
    }

    if((acfg_ctrl_req(ifname, cmd, strlen(cmd),
                    replybuf, &len, opmode) < 0) ||
            strncmp(replybuf, "OK", strlen("OK"))){
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

int acfg_get_legacy_rate(int rate)
{
    unsigned int i = 0;
    int legacy_rate_idx[][2] = {
        {1, 0x1b},
        {2, 0x1a},
        {5, 0x19},
        {6, 0xb},
        {9, 0xf},
        {11, 0x18},
        {12, 0xa},
        {18, 0xe},
        {24, 0x9},
        {36, 0xd},
        {48, 0x8},
        {54, 0xc},
    };
    for (i = 0; i < (sizeof(legacy_rate_idx)/sizeof(legacy_rate_idx[0])); i++)
    {
        if (legacy_rate_idx[i][0] == rate) {
            return legacy_rate_idx[i][1];
        }
    }
    return 0;
}

int acfg_get_mcs_rate(int val)
{
    unsigned int i = 0;
    int mcs_rate_idx[][2] = {
        {0, 0x80},
        {1, 0x81},
        {2, 0x82},
        {3, 0x83},
        {4, 0x84},
        {5, 0x85},
        {6, 0x86},
        {7, 0x87},
        {8, 0x88},
        {9, 0x89},
        {10, 0x8a},
        {11, 0x8b},
        {12, 0x8c},
        {13, 0x8d},
        {14, 0x8e},
        {15, 0x8f},
        {16, 0x90},
        {17, 0x91},
        {18, 0x92},
        {19, 0x93},
        {20, 0x94},
        {21, 0x95},
        {22, 0x96},
        {23, 0x97},
    };

    if (val >= (int)(sizeof(mcs_rate_idx)/sizeof(mcs_rate_idx[0]))) {
        return 0;
    }
    for (i = 0; i < sizeof(mcs_rate_idx)/sizeof(mcs_rate_idx[0]); i++)
    {
        if (mcs_rate_idx[i][0] == val) {
            return mcs_rate_idx[i][1];
        }
    }
    return 0;
}

void
acfg_parse_rate(uint8_t *rate_str, int *val)
{
    char *pos = NULL, *start;
    char buf[16];
    int rate = 0;
    int ratecode, i;
    int maxlen = 0;

    start = (char *)rate_str;
    pos = strchr((char *)rate_str, 'M');
    if (pos) {
        if((unsigned int)(pos - start) >= sizeof(buf))
            maxlen = sizeof(buf);
        else
            maxlen = pos - start + 1;
        acfg_os_strcpy((uint8_t *)buf, (uint8_t *)start, maxlen);
        rate = atoi(buf);
        ratecode = acfg_get_legacy_rate(rate);
    } else {
        acfg_os_strcpy((uint8_t *)buf, (uint8_t *)start, sizeof(buf));
        rate = atoi(buf);
        rate = rate - 1;
        if (rate < 0) {
            *val = 0;
            return;
        }
        ratecode = acfg_get_mcs_rate(rate);
    }
    *val = 0;
    for (i = 0; i < 4; i++) {
        *val |= ratecode << (i * 8);
    }
}

uint32_t
acfg_wlan_vap_profile_vlan_add(acfg_wlan_profile_vap_params_t *vap_params)
{
    char str[60];
    char iface_name[ACFG_MAX_IFNAME];
    char vlan_bridge[ACFG_MAX_IFNAME];
    uint32_t status = QDF_STATUS_SUCCESS;
    int ret = 0;

    status = acfg_wlan_iface_present("eth0");
    if (status == QDF_STATUS_SUCCESS) {
        snprintf(str, sizeof(str), "brctl delif br0 eth0");
        ret = system(str);
    }

    status = acfg_wlan_iface_present("eth1");
    if (status == QDF_STATUS_SUCCESS) {
        snprintf(str, sizeof(str), "brctl delif br0 eth1");
        ret = system(str);
    }

    status = acfg_wlan_iface_present((char *)vap_params->vap_name);
    if (status == QDF_STATUS_SUCCESS) {
        snprintf(str, sizeof(str), "brctl delif br0 %s", vap_params->vap_name);
        ret = system(str);
    }

    snprintf(vlan_bridge, sizeof(vlan_bridge), "br%d", vap_params->vlanid);
    status = acfg_wlan_iface_present(vlan_bridge);
    if (status != QDF_STATUS_SUCCESS) {
        snprintf(str, sizeof(str), "brctl addbr %s", vlan_bridge);
        ret = system(str);
    }

    snprintf(str, sizeof(str), "brctl delif br%d %s",
             vap_params->vlanid, vap_params->vap_name);
    ret = system(str);

    snprintf(str, sizeof(str), "vconfig add %s %d", vap_params->vap_name,
             vap_params->vlanid);
    ret = system(str);

    snprintf(str, sizeof(str), "vconfig add eth0 %d", vap_params->vlanid);
    ret = system(str);

    snprintf(str, sizeof(str), "vconfig add eth1 %d", vap_params->vlanid);
    ret = system(str);

    snprintf(str, sizeof(str), "brctl addif %s %s.%d", vlan_bridge,
            vap_params->vap_name, vap_params->vlanid);
    ret = system(str);
    snprintf(str, sizeof(str), "brctl addif %s eth0.%d", vlan_bridge,
            vap_params->vlanid);
    ret = system(str);
    snprintf(str, sizeof(str), "brctl addif %s eth1.%d", vlan_bridge,
            vap_params->vlanid);
    ret = system(str);

    if (ret) {
        status = ret;
        return status;
    }

    snprintf(iface_name, sizeof(iface_name), "%s.%d", vap_params->vap_name, vap_params->vlanid);
    status = acfg_wlan_iface_up((uint8_t *)iface_name, NULL);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("Failed to bring vap UP\n");
        return status;
    }
    snprintf(iface_name, sizeof(iface_name), "eth0.%d", vap_params->vlanid);
    status = acfg_wlan_iface_up((uint8_t *)iface_name, NULL);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("Failed to bring %s UP\n", str);
        return status;
    }
    snprintf(iface_name, sizeof(iface_name), "eth1.%d", vap_params->vlanid);
    status = acfg_wlan_iface_up((uint8_t *)iface_name, NULL);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("Failed to bring %s UP\n", str);
        return status;
    }
    return status;
}

void acfg_wlan_vap_profile_vlan_remove(acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    char str[60];
    char iface_name[ACFG_MAX_IFNAME];
    char vlan_bridge[ACFG_MAX_IFNAME];
    uint32_t status = QDF_STATUS_SUCCESS;
    int ret = 0;

    snprintf(vlan_bridge, sizeof(vlan_bridge), "br%d", cur_vap_params->vlanid);

    snprintf(iface_name, sizeof(iface_name), "%s.%d", cur_vap_params->vap_name, cur_vap_params->vlanid);
    status = acfg_wlan_iface_present(iface_name);
    if (status == QDF_STATUS_SUCCESS) {
        acfg_wlan_iface_down((uint8_t *)iface_name, NULL);
        snprintf(str,sizeof(str), "brctl delif %s %s", vlan_bridge, iface_name);
        ret = system(str);
    }

    snprintf(str, sizeof(str), "vconfig rem %s.%d", cur_vap_params->vap_name,
            cur_vap_params->vlanid);
    ret = system(str);
    if (ret)
        printf("system call failed.\n");
    return;
}

uint32_t
acfg_percent_str_to_octet(char *tmp_new, char end_chr, uint32_t *percent)
{
    char *str = tmp_new;
    int8_t cnt = 0;

    while((*str != end_chr) && (*str != '\0') && (cnt < 4))
    {
        cnt++;
        str++;
    }

    if(cnt == 4)
    {
        acfg_log_errstr("Percent value invalid\n");
        return QDF_STATUS_E_FAILURE;
    }

    str = tmp_new;
    *percent = 0;

    while(cnt--)
    {
        if((*str >= '0')&&(*str <= '9'))
        {
            *percent = (*percent * 10) + (*str - '0');
            str++;
        }
        else{
            acfg_log_errstr("Percent range should be in decimal, but %c \n", *str);
            return QDF_STATUS_E_FAILURE;
        }
    }
    if(*percent > 100)
    {
        acfg_log_errstr("Percent range should be btw 0 ~ 100, but %d\n", *percent);
        return QDF_STATUS_E_FAILURE;
    }
    else
        return QDF_STATUS_SUCCESS;
}

uint32_t
acfg_vap_atf_addssid(uint8_t *vap_name,
        char *ssid,
        uint32_t percent)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SET_ATF_ADDSSID};
    acfg_atf_ssid_val_t *ptr = (acfg_atf_ssid_val_t *)req.data;

    memset(ptr, 0, sizeof(acfg_atf_ssid_val_t));

    acfg_os_strcpy((uint8_t *)ptr->ssid, (uint8_t *)ssid, ACFG_MAX_SSID_LEN + 1);
    ptr->value = percent * 10;

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

uint32_t
acfg_vap_atf_delssid(uint8_t *vap_name,
        char *ssid)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SET_ATF_DELSSID};
    acfg_atf_ssid_val_t *ptr = (acfg_atf_ssid_val_t *)req.data;

    memset(ptr, 0, sizeof(acfg_atf_ssid_val_t));

    acfg_os_strcpy((uint8_t *)ptr->ssid, (uint8_t *)ssid, ACFG_MAX_SSID_LEN + 1);

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

uint32_t
acfg_vap_atf_addsta(uint8_t *vap_name,
        uint8_t *mac,
        uint32_t percent)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SET_ATF_ADDSTA};
    acfg_atf_sta_val_t *ptr = (acfg_atf_sta_val_t *)req.data;

    memset(ptr, 0, sizeof(acfg_atf_sta_val_t));

    memcpy(ptr->sta_mac, mac, ACFG_MACADDR_LEN);
    ptr->value = percent * 10;

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

uint32_t
acfg_vap_atf_delsta(uint8_t *vap_name,
        uint8_t *mac)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SET_ATF_DELSTA};
    acfg_atf_sta_val_t *ptr = (acfg_atf_sta_val_t *)req.data;

    memset(ptr, 0, sizeof(acfg_atf_sta_val_t));

    memcpy(ptr->sta_mac, mac, ACFG_MACADDR_LEN);

    status = acfg_os_send_req(vap_name, &req);

    return status;
}

#if QCA_AIRTIME_FAIRNESS
uint32_t
acfg_vap_atf_commit(uint8_t *vap_name)
{
    uint32_t status = QDF_STATUS_SUCCESS;

    status = acfg_set_vap_param(vap_name,
            ACFG_PARAM_ATF_OPT,
            1);
    if (status != QDF_STATUS_SUCCESS)
    {
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

uint32_t
acfg_vap_configure_atf(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    uint8_t update = 0;

    if(strncmp(vap_params->atf.atf_percent,
                cur_vap_params->atf.atf_percent,
                ACFG_MAX_PERCENT_SIZE))
    {
        /* NULL, Remove the ssid from the ATF table */
        if(vap_params->atf.atf_percent[0] == '\0')
        {
            status = acfg_vap_atf_delssid(vap_params->vap_name, vap_params->ssid);
            if(status != QDF_STATUS_SUCCESS)
            {
                acfg_log_errstr("ATF delssid failed %s\n", vap_params->vap_name);
                return QDF_STATUS_E_FAILURE;
            }
        }
        /* Add the new percent to ATF table */
        else
        {
            uint32_t percent;

            status = acfg_percent_str_to_octet(vap_params->atf.atf_percent,
                    '\0', &percent);
            if(status != QDF_STATUS_SUCCESS)
            {
                acfg_log_errstr("Invalid VAP ATF percent\n");
                return QDF_STATUS_E_FAILURE;
            }
            status = acfg_vap_atf_addssid(vap_params->vap_name,
                    vap_params->ssid,
                    percent);
            if(status != QDF_STATUS_SUCCESS)
            {
                acfg_log_errstr("ATF addssid failed %s\n", vap_params->vap_name);
                return QDF_STATUS_E_FAILURE;
            }
        }
        update = 1;
    }
    if(strncmp(vap_params->atf.atf_stalist,
                cur_vap_params->atf.atf_stalist,
                ACFG_MAX_ATF_STALIST_SIZE))
    {
        uint8_t mac[ACFG_MACADDR_LEN];
        char  mac_str[18] = {0};

        if(strchr(cur_vap_params->atf.atf_stalist, ':'))
        {
            char *old = cur_vap_params->atf.atf_stalist;

            /* Remove old entries from the ATF table */
            for(;;)
            {
                uint8_t mac[ACFG_MACADDR_LEN];

                memcpy(mac_str, old, 17);
                acfg_mac_str_to_octet((uint8_t *)mac_str, mac);

                status = acfg_vap_atf_delsta(vap_params->vap_name, mac);
                if(status != QDF_STATUS_SUCCESS)
                {
                    acfg_log_errstr("ATF delsta failed\n");
                    return QDF_STATUS_E_FAILURE;
                }

                /* Point to the next old entry */
                old = strchr(old, ';');
                if(old == NULL)
                    break;
                old++;

            }
        }

        /* Add new entries to the ATF table */
        if(strchr(vap_params->atf.atf_stalist, ':'))
        {
            char *new = vap_params->atf.atf_stalist;
            char *percent_str;
            uint32_t percent;

            for(;;)
            {
                memcpy(mac_str, new, 17);
                acfg_mac_str_to_octet((uint8_t *)mac_str, mac);

                percent_str = strchr(new, ',');
                if(percent_str == NULL)
                {
                    acfg_log_errstr("No STA ATF percent provided\n");
                    return QDF_STATUS_E_FAILURE;
                }
                percent_str++;

                status = acfg_percent_str_to_octet(percent_str, ';', &percent);
                if(status != QDF_STATUS_SUCCESS)
                {
                    acfg_log_errstr("Invalid STA ATF percent\n");
                    return QDF_STATUS_E_FAILURE;
                }
                status  = acfg_vap_atf_addsta(vap_params->vap_name, mac, percent);
                if(status != QDF_STATUS_SUCCESS)
                {
                    acfg_log_errstr("ATF addsta failed\n");
                    return QDF_STATUS_E_FAILURE;
                }
                /* Point to the next new entry */
                new = strchr(new, ';');
                if(new == NULL)
                    break;
                new++;
            }
        }
        update = 1;
    }
    if(update)
    {
        /* Now commit the ATF change */
        status = acfg_vap_atf_commit(vap_params->vap_name);
        if(status != QDF_STATUS_SUCCESS)
        {
            acfg_log_errstr("ATF commit failed\n");
            return QDF_STATUS_E_FAILURE;
        }
    }

    return QDF_STATUS_SUCCESS;
}
#endif

uint32_t
acfg_wlan_vap_create(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_radio_params_t radio_params)
{
    uint32_t status = QDF_STATUS_SUCCESS;

    status = acfg_wlan_iface_present((char *)vap_params->vap_name);
    if(status == QDF_STATUS_SUCCESS) {
        acfg_log_errstr("Interface Already present\n");
        return QDF_STATUS_E_INVAL;
    }
    if ((vap_params->opmode == ACFG_OPMODE_STA) &&
            (vap_params->wds_params.wds_flags != ACFG_FLAG_VAP_ENHIND))
    {
        if((vap_params->vapid == VAP_ID_AUTO) || (radio_params.macreq_enabled != 1))
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    IEEE80211_CLONE_BSSID | IEEE80211_CLONE_NOBEACONS);
        else
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    IEEE80211_CLONE_NOBEACONS);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to Create Vap %s\n", vap_params->vap_name);
            return QDF_STATUS_E_FAILURE;
        }
    }
    else
    {
        if((vap_params->vapid == VAP_ID_AUTO) || (radio_params.macreq_enabled != 1))
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    IEEE80211_CLONE_BSSID);
        else
            status = acfg_create_vap(radio_params.radio_name,
                    vap_params->vap_name,
                    vap_params->opmode,
                    vap_params->vapid,
                    0);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to Create Vap %s\n", vap_params->vap_name);
            return QDF_STATUS_E_FAILURE;
        }
    }
    return status;
}

uint32_t
acfg_wlan_vap_profile_modify(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        acfg_wlan_profile_radio_params_t radio_params)
{
    acfg_ssid_t ssid;
    acfg_rate_t rate;
    int8_t sec = 0;
    int if_down = 0, setssid = 0, enablewep = 0, set_open = 0,
        set_wep = 0, wps_state = 0;
    uint32_t status = QDF_STATUS_SUCCESS;
    uint8_t mac[ACFG_MACADDR_LEN];
    char str[60];
    int rate_val = 0, retries = 0, i;
    int ret = 0;

    (void) radio_params;

    if(vap_params->opmode != cur_vap_params->opmode) {
        acfg_log_errstr("Operating Mode cannot be modified\n");
        return QDF_STATUS_E_FAILURE;
    }

    if ((vap_params->vlanid) && (vap_params->vlanid != ACFG_WLAN_PROFILE_VLAN_INVALID)) {
        snprintf((char *)vap_params->bridge,
                 sizeof(vap_params->bridge), "br%d", vap_params->vlanid);
     }
    if ((cur_vap_params->vlanid) && (vap_params->vlanid != ACFG_WLAN_PROFILE_VLAN_INVALID)) {
        snprintf((char *)cur_vap_params->bridge,
                 sizeof(cur_vap_params->bridge), "br%d", cur_vap_params->vlanid);
    }

    if (!ACFG_STR_MATCH(vap_params->ssid, cur_vap_params->ssid)) {
        memcpy(ssid.ssid, vap_params->ssid, (ACFG_MAX_SSID_LEN + 1));
        if(strlen((char *)ssid.ssid) > 0) {
            if (!if_down) {
                status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
                if_down = 1;
                setssid = 1;
                if(status != QDF_STATUS_SUCCESS) {
                    return QDF_STATUS_E_FAILURE;
                }
            }
            status = acfg_set_ssid(vap_params->vap_name, &ssid);
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
    }

    if (!ACFG_STR_MATCH(vap_params->bridge, cur_vap_params->bridge)) {
        if (vap_params->bridge[0] == 0) {
            status = acfg_wlan_iface_present(cur_vap_params->bridge);

            if (status == QDF_STATUS_SUCCESS) {
                snprintf(str, sizeof(str),
                         "brctl delif %s %s", cur_vap_params->bridge,
                         vap_params->vap_name);
                ret = system(str);
            }
        } else if (!cur_vap_params->bridge[0] && vap_params->bridge[0]) {
            status = acfg_wlan_iface_present(vap_params->bridge);

            if (status != QDF_STATUS_SUCCESS) {
                snprintf(str, sizeof(str), "brctl addbr %s", vap_params->bridge);
                ret = system(str);
            }

            status = acfg_wlan_iface_up((uint8_t *)vap_params->bridge, NULL);

            snprintf(str, sizeof(str), "brctl addif %s %s", vap_params->bridge,
                    vap_params->vap_name);
            ret = system(str);
            snprintf(str, sizeof(str), "brctl setfd %s 0", vap_params->bridge);
            ret = system(str);
        } else if (cur_vap_params->bridge[0] && vap_params->bridge[0]) {
            status = acfg_wlan_iface_present(cur_vap_params->bridge);
            if (status == QDF_STATUS_SUCCESS) {
                snprintf(str, sizeof(str),
                         "brctl delif %s %s", cur_vap_params->bridge,
                         vap_params->vap_name);
                ret = system(str);
            }
            status = acfg_wlan_iface_present(vap_params->bridge);

            if (status != QDF_STATUS_SUCCESS) {
                snprintf(str, sizeof(str), "brctl addbr %s",
                         vap_params->bridge);
                ret = system(str);
            }
            snprintf(str, sizeof(str), "brctl addif %s %s", vap_params->bridge,
                    vap_params->vap_name);
            status = acfg_wlan_iface_up((uint8_t *)vap_params->bridge, NULL);
            ret = system(str);
            snprintf(str, sizeof(str), "brctl setfd %s 0", vap_params->bridge);
            ret = system(str);
        }
    }
    if (ret) {
        printf("some system call failed\n");
        return QDF_STATUS_E_FAILURE;
    }
    if (vap_params->wds_params.enabled != cur_vap_params->wds_params.enabled) {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
            if_down = 1;
            setssid = 1;
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_WDS,
                vap_params->wds_params.enabled);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to enbale wds\n");
            return QDF_STATUS_E_FAILURE;
        }
    }

    if (vap_params->wds_params.wds_flags !=
            cur_vap_params->wds_params.wds_flags)
    {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
            if_down = 1;
            setssid = 1;
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }


        if ((vap_params->wds_params.wds_flags & ACFG_FLAG_VAP_ENHIND) ==
                ACFG_FLAG_VAP_ENHIND)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_VAP_ENHIND, 1);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed to set wds repeater Enhanced independent flag\n");
                return QDF_STATUS_E_FAILURE;
            }
        }
        else
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_VAP_ENHIND, 0);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed to set wds repeater Enhanced independent flag\n");
                return QDF_STATUS_E_FAILURE;
            }
        }

        if (vap_params->opmode == ACFG_OPMODE_STA) {
            if ((vap_params->wds_params.wds_flags & ACFG_FLAG_EXTAP) ==
                    ACFG_FLAG_EXTAP)
            {
                status = acfg_set_vap_param(vap_params->vap_name,
                        ACFG_PARAM_EXTAP, 1);
                if (status != QDF_STATUS_SUCCESS) {
                    acfg_log_errstr("Failed to set wds extension flag\n");
                    return QDF_STATUS_E_FAILURE;
                }
            }
            else
            {
                status = acfg_set_vap_param(vap_params->vap_name,
                        ACFG_PARAM_EXTAP, 0);
                if (status != QDF_STATUS_SUCCESS) {
                    acfg_log_errstr("Failed to set wds extension flag\n");
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
    }

    if (vap_params->phymode != cur_vap_params->phymode) {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
            if_down = 1;
            setssid = 1;
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
        status = acfg_set_phymode(vap_params->vap_name,
                vap_params->phymode);
        if(status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if(vap_params->opmode == ACFG_OPMODE_HOSTAP) {
        if (vap_params->primary_vap && (vap_params->beacon_interval != cur_vap_params->beacon_interval))
        {
            if (!if_down) {
                status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
                if_down = 1;
                setssid = 1;
                if(status != QDF_STATUS_SUCCESS) {
                    return QDF_STATUS_E_FAILURE;
                }
            }
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_BEACON_INTERVAL,
                    vap_params->beacon_interval);
            if(status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed to set beacon interval\n");
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
    if(vap_params->opmode == ACFG_OPMODE_HOSTAP) {
        if (vap_params->dtim_period != cur_vap_params->dtim_period)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_DTIM_PERIOD,
                    vap_params->dtim_period);
            if(status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed to set dtim period\n");
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
    if(vap_params->bitrate != cur_vap_params->bitrate) {
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
            if_down = 1;
            setssid = 1;
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
        rate = vap_params->bitrate;
        status = acfg_set_rate(vap_params->vap_name, &rate);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set rate\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (!ACFG_STR_MATCH(vap_params->rate, cur_vap_params->rate)) {
        acfg_parse_rate(vap_params->rate, &rate_val);
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_11N_RATE,
                rate_val);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set rate\n");
        }
    }
    if (vap_params->retries != cur_vap_params->retries) {
        for (i = 0; i < 4; i++) {
            retries |= vap_params->retries << (i * 8);
        }
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_11N_RETRIES,
                retries);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set retries\n");
        }
    }
    if(vap_params->frag_thresh !=
            cur_vap_params->frag_thresh)
    {

        if((vap_params->frag_thresh >= IEEE80211_FRAGMT_THRESHOLD_MAX) ||
                (vap_params->frag_thresh == 0))
        {
            vap_params->frag_thresh = IEEE80211_FRAGMT_THRESHOLD_MAX;
        }

        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
            if_down = 1;
            setssid = 1;
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
        status = acfg_set_frag(vap_params->vap_name,
                &vap_params->frag_thresh);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set fragmentation Threshold\n");
        }
    }
    if(vap_params->rts_thresh !=
            cur_vap_params->rts_thresh)
    {

        if((vap_params->rts_thresh >= IEEE80211_RTS_MAX) ||
                (vap_params->rts_thresh == 0))
        {
            vap_params->rts_thresh = IEEE80211_RTS_MAX;
        }
        if (!if_down) {
            status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
            if_down = 1;
            setssid = 1;
            if(status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
        status = acfg_set_rts(vap_params->vap_name,
                &vap_params->rts_thresh);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set rts threshold\n");
        }
    }
    status = acfg_set_node_list(vap_params, cur_vap_params);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to set node list (vap=%s status=%d)!\n",
                __func__, vap_params->vap_name, status);
        return QDF_STATUS_E_FAILURE;
    }
    status = acfg_set_acl_policy(vap_params, cur_vap_params);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to set ACL policy (vap=%s status=%d)!\n",
                __func__, vap_params->vap_name, status);
        return QDF_STATUS_E_FAILURE;
    }
    /* For Second ACL list */
    status = acfg_set_node_list_secondary(vap_params, cur_vap_params);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to set node list (vap=%s status=%d)!\n",
                __func__, vap_params->vap_name, status);
        return QDF_STATUS_E_FAILURE;
    }
    status = acfg_set_acl_policy_secondary(vap_params, cur_vap_params);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to set ACL policy (vap=%s status=%d)!\n",
                __func__, vap_params->vap_name, status);
        return QDF_STATUS_E_FAILURE;
    }

    if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) &&
            (vap_params->pureg != cur_vap_params->pureg))
    {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_PUREG,
                vap_params->pureg);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set pureg\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) &&
            (vap_params->puren != cur_vap_params->puren))
    {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_PUREN,
                vap_params->puren);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set puren\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) &&
            (vap_params->hide_ssid !=
             cur_vap_params->hide_ssid))
    {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_HIDE_SSID,
                vap_params->hide_ssid);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set hide ssid param\n");
            return QDF_STATUS_E_FAILURE;
        }

    }
    if ((vap_params->opmode == ACFG_OPMODE_HOSTAP) &&
            (vap_params->bcn_rate != cur_vap_params->bcn_rate )) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_MODIFY_BEACON_RATE,
                vap_params->bcn_rate);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set bcn_rate param\n");
            return QDF_STATUS_E_FAILURE;
        }

    }
    if (vap_params->implicitbf != cur_vap_params->implicitbf) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_IMPLICITBF,
                vap_params->implicitbf);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set implicitbf param\n");
            return QDF_STATUS_E_FAILURE;
        }

    }
    if (vap_params->wnm != cur_vap_params->wnm) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_WNM_ENABLE,
                vap_params->wnm);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set wnm param\n");
            return QDF_STATUS_E_FAILURE;
        }
    }

    if (vap_params->rrm != cur_vap_params->rrm) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_RRM_CAP,
                vap_params->rrm);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set rrm param\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->primary_vap && (vap_params->doth != cur_vap_params->doth)) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_DOTH,
                vap_params->doth);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set hide doth param\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->primary_vap && (vap_params->coext != cur_vap_params->coext)) {
        status = acfg_set_vap_param(vap_params->vap_name,
                        ACFG_PARAM_COEXT_DISABLE,
                        !vap_params->coext);
        if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed to set coext param\n");
                return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->client_isolation != cur_vap_params->client_isolation) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_APBRIDGE,
                !vap_params->client_isolation);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set ap bridge param\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->ampdu != cur_vap_params->ampdu) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_VAP_AMPDU,
                vap_params->ampdu);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set ampdu param\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->uapsd != cur_vap_params->uapsd) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_UAPSD,
                vap_params->uapsd);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set uapsd\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->shortgi != cur_vap_params->shortgi) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_VAP_SHORT_GI,
                vap_params->shortgi);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set shortgi\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->amsdu != cur_vap_params->amsdu) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_AMSDU,
                vap_params->amsdu);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set amsdu\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->max_clients != cur_vap_params->max_clients) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_MAXSTA,
                vap_params->max_clients);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set max_clients\n");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (vap_params->atf_options != cur_vap_params->atf_options) {
        status = acfg_set_vap_param(vap_params->vap_name,
                ACFG_PARAM_ATF_MAX_CLIENT,
                vap_params->atf_options);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set max_clients\n");
            return QDF_STATUS_E_FAILURE;
        }
    }

    if((vap_params->security_params.hs_iw_param.hs_enabled == 1) &&
            (vap_params->security_params.hs_iw_param.iw_enabled == 1))
    {
        acfg_set_hs_iw_vap_param(vap_params);
    }

    //Set security parameters
    if (vap_params->security_params.wps_flag == 0) {
        acfg_rem_wps_config_file(vap_params->vap_name);
    } else if (ACFG_SEC_CMP(vap_params, cur_vap_params)) {
        acfg_rem_wps_config_file(vap_params->vap_name);
    } else {
        acfg_wps_cred_t wps_cred;
        memset(&wps_cred, 0x00, sizeof(wps_cred));
        /* Check & Set default WPS dev params */
        acfg_set_wps_default_config(vap_params);
        /* Update/create the WPS config file*/
        acfg_update_wps_dev_config_file(vap_params, 0);

        wps_state = acfg_get_wps_config(vap_params->vap_name, &wps_cred);
        if (wps_state == 1) {
            status = acfg_set_wps_vap_params(vap_params, &wps_cred);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("%s: Failed to set WPS VAP params (vap=%s status=%d)!\n",
                        __func__, vap_params->vap_name, status);
                return QDF_STATUS_E_FAILURE;
            }
        }
    }

    if(vap_params->num_vendor_params != 0)
    {
        int i, j, configure;
        for(i = 0; i < vap_params->num_vendor_params; i++)
        {
            configure = 1;

            for(j = 0; j < cur_vap_params->num_vendor_params; j++)
            {
                if(vap_params->vendor_param[i].cmd ==
                        cur_vap_params->vendor_param[j].cmd)
                {
                    int len = 0;

                    if(vap_params->vendor_param[i].len == cur_vap_params->vendor_param[j].len)
                    {
                        /* Length is equal, check data */
                        len = vap_params->vendor_param[i].len;
                        if(0 == memcmp((void *)&vap_params->vendor_param[i].data,
                                    (void *)&cur_vap_params->vendor_param[j].data,
                                    len))
                        {
                            /* Data is same, No need to configure again */
                            configure = 0;
                        }
                        else
                        {
                            /* Data is different, Need to configure again */
                            configure = 1;
                        }
                    }
                }
            }
            if(configure == 1)
            {
                status = acfg_set_vap_vendor_param(vap_params->vap_name,
                        vap_params->vendor_param[i].cmd,
                        (uint8_t *)&vap_params->vendor_param[i].data,
                        vap_params->vendor_param[i].len,
                        vap_params->vendor_param[i].type,
                        0);
                if (status != QDF_STATUS_SUCCESS)
                {
                    acfg_log_errstr("Failed to set vendor param: status %d\n", status);
                    return QDF_STATUS_E_FAILURE;
                }
            }
        }
    }
    if(QDF_STATUS_SUCCESS != acfg_set_security(vap_params, cur_vap_params,
                PROFILE_MODIFY, &sec)){
        acfg_log_errstr("%s: Failed to set %s security params\n", __func__,
                vap_params->vap_name);
        return QDF_STATUS_E_FAILURE;
    }
    if (vap_params->security_params.sec_method !=
            cur_vap_params->security_params.sec_method && (sec != 1))
    {
        if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_SHARED)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_AUTHMODE, 2);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed Set vap param\n");
                return QDF_STATUS_E_FAILURE;
            }
            enablewep = 1;
        } else if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_AUTO)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_AUTHMODE, 4);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed Set vap param\n");
                return QDF_STATUS_E_FAILURE;
            }
            enablewep = 1;
        } else if (vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN)
        {
            status = acfg_set_vap_param(vap_params->vap_name,
                    ACFG_PARAM_AUTHMODE, 1);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed Set vap param\n");
                return QDF_STATUS_E_FAILURE;
            }
            enablewep = 1;
        } else if (vap_params->security_params.sec_method >=
                ACFG_WLAN_PROFILE_SEC_METH_INVALID)
        {
            acfg_log_errstr("Invalid Security Method \n\r");
            return QDF_STATUS_E_FAILURE;
        }
    }
    set_wep = ((vap_params->security_params.cipher_method ==
                ACFG_WLAN_PROFILE_CIPHER_METH_WEP) &&
            ((vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_SHARED) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_AUTO)));
    if (set_wep)
    {
        int flag = 0;
        if (vap_params->security_params.cipher_method !=
                cur_vap_params->security_params.cipher_method)
        {
            enablewep = 1;
            setssid = 1;
        }

        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key0,
                    cur_vap_params->security_params.wep_key0) ||
                enablewep)
        {
            if (vap_params->security_params.wep_key0[0] != '\0') {
                flag = 1;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key0);
                if (status != QDF_STATUS_SUCCESS) {
                    acfg_log_errstr("Failed to set enc\n");
                    return QDF_STATUS_E_FAILURE;
                }
            }
            setssid = 1;
        }
        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key1,
                    cur_vap_params->security_params.wep_key1) ||
                enablewep)
        {
            if(vap_params->security_params.wep_key1[0] != '\0') {
                flag = 2;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key1);
                if (status != QDF_STATUS_SUCCESS) {
                    acfg_log_errstr("Failed to set enc\n");
                    return QDF_STATUS_E_FAILURE;
                }
            }
            setssid = 1;
        }
        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key2,
                    cur_vap_params->security_params.wep_key2) ||
                enablewep)
        {
            if(vap_params->security_params.wep_key2[0] != '\0') {
                flag = 3;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key2);
                if (status != QDF_STATUS_SUCCESS) {
                    acfg_log_errstr("Failed to set enc\n");
                    return QDF_STATUS_E_FAILURE;
                }
            }
            setssid = 1;
        }
        if (!ACFG_STR_MATCH(vap_params->security_params.wep_key3,
                    cur_vap_params->security_params.wep_key3) ||
                enablewep)
        {
            if(vap_params->security_params.wep_key3[0] != '\0') {
                flag = 4;
                status = acfg_set_enc(vap_params->vap_name, flag,
                                        vap_params->security_params.wep_key3);
                if (status != QDF_STATUS_SUCCESS) {
                    acfg_log_errstr("Failed to set enc\n");
                    return QDF_STATUS_E_FAILURE;
                }
            }
            setssid = 1;
        }
        //Set default key idx
        if ((vap_params->security_params.wep_key_defidx != 0)) {
            if ((vap_params->security_params.wep_key_defidx !=
                        cur_vap_params->security_params.wep_key_defidx) ||
                    enablewep)
            {
                flag = vap_params->security_params.wep_key_defidx;
                status = acfg_set_enc(vap_params->vap_name, flag, 0);
                if (status != QDF_STATUS_SUCCESS) {
                    acfg_log_errstr("Failed to set enc\n");
                    return QDF_STATUS_E_FAILURE;
                }
                setssid = 1;
            }
        }
    }
    if((vap_params->security_params.sec_method ==
                ACFG_WLAN_PROFILE_SEC_METH_OPEN) && (sec != 1))
    {
        if (vap_params->security_params.sec_method !=
                cur_vap_params->security_params.sec_method)
        {
            if (vap_params->security_params.cipher_method ==
                    ACFG_WLAN_PROFILE_CIPHER_METH_NONE)
            {
                set_open = 1;
            }
        }
        if (vap_params->security_params.cipher_method !=
                cur_vap_params->security_params.cipher_method)
        {
            if (vap_params->security_params.cipher_method ==
                    ACFG_WLAN_PROFILE_CIPHER_METH_NONE)
            {
                set_open = 1;
            }
        }
        if (set_open) {
            status = acfg_set_auth_open(vap_params, ACFG_SEC_DISABLE_SECURITY);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("%s: Failed to set auth to open (vap=%s status=%d)!\n",
                        __func__, vap_params->vap_name, status);
                return QDF_STATUS_E_FAILURE;
            }
        }
        if (vap_params->security_params.sec_method !=
                cur_vap_params->security_params.sec_method)
        {
            setssid = 1;
        }

    }
    if ((vap_params->opmode == ACFG_OPMODE_STA) &&
            ((vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_SHARED) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_AUTO)) &&
            (vap_params->wds_params.enabled == 1))
    {
        if ((!ACFG_STR_MATCH(vap_params->wds_params.wds_addr,
                        cur_vap_params->wds_params.wds_addr)) && \
                (vap_params->wds_params.wds_addr[0] != 0))
        {
            acfg_mac_str_to_octet(vap_params->wds_params.wds_addr, mac);

            status = acfg_set_ap(vap_params->vap_name, mac);

            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("Failed to set ROOTAP MAC\n");
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
    if((vap_params->txpow != cur_vap_params->txpow) &&
            (vap_params->opmode == ACFG_OPMODE_HOSTAP)) {
        status = acfg_set_txpow(vap_params->vap_name,
                &vap_params->txpow);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set txpower\n");
            return QDF_STATUS_E_FAILURE;
        }
    }

    if (vap_params->vlanid != cur_vap_params->vlanid) {
        if ((cur_vap_params->vlanid == 0) && (vap_params->vlanid != 0)) {
            status = acfg_wlan_vap_profile_vlan_add(vap_params);
            if(status != QDF_STATUS_SUCCESS){
                acfg_log_errstr("Failed to add %s to vlan\n", vap_params->vap_name);
                return QDF_STATUS_E_FAILURE;
            }
        } else if ((cur_vap_params->vlanid != 0) && (vap_params->vlanid == 0)) {
            acfg_wlan_vap_profile_vlan_remove(cur_vap_params);
        } else {
            acfg_wlan_vap_profile_vlan_remove(cur_vap_params);
            status = acfg_wlan_vap_profile_vlan_add(vap_params);
            if(status != QDF_STATUS_SUCCESS){
                acfg_log_errstr("Failed to add %s to vlan\n", vap_params->vap_name);
                return QDF_STATUS_E_FAILURE;
            }
        }
    }

    //Set the ssid if in Station mode and security mode is open or wep
    if ((vap_params->opmode == ACFG_OPMODE_STA) && (setssid) &&
            ((vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_OPEN) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_SHARED) ||
             (vap_params->security_params.sec_method ==
              ACFG_WLAN_PROFILE_SEC_METH_AUTO)))
    {
        acfg_os_strcpy((uint8_t *)ssid.ssid, (uint8_t *)vap_params->ssid,
                       (ACFG_MAX_SSID_LEN + 1));
        status = acfg_set_ssid(vap_params->vap_name, &ssid);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to set the SSID\n");
            return QDF_STATUS_E_FAILURE;
        }
    }

#if QCA_AIRTIME_FAIRNESS
    status = acfg_vap_configure_atf(vap_params, cur_vap_params);
#endif
    return status;
}

uint32_t
acfg_wlan_vap_profile_delete(acfg_wlan_profile_vap_params_t *vap_params)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    uint8_t *vapname = vap_params->vap_name;
    char *radioname = vap_params->radio_name;
    int8_t sec;

    status = acfg_get_opmode(vap_params->vap_name,
            &vap_params->opmode);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Opmode fetch fail\n", vap_params->vap_name);
    }

    if(acfg_set_security(vap_params, NULL,
                PROFILE_DELETE, &sec) != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to delete %s security params\n", __func__,
                vap_params->vap_name);
        return QDF_STATUS_E_INVAL;
    }

    if( (*vapname) && (*radioname)) {

        status = acfg_wlan_iface_present(radioname);

        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Radio Interface not present %d \n",  status);
            return QDF_STATUS_E_INVAL;
        }

        status = acfg_wlan_iface_present((char *)vapname);

        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Vap is Not Present!!\n");
            return QDF_STATUS_E_FAILURE;
        }

        status = acfg_delete_vap((uint8_t *)radioname, vapname);

        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("Failed to delete vap!\n\a\a");
        }
    }
    return status;
}

void
acfg_mac_to_str(uint8_t *addr, char *str, uint16_t str_max_len)
{
    snprintf(str, str_max_len, "%02x:%02x:%02x:%02x:%02x:%02x", addr[0],
             addr[1],
             addr[2],
             addr[3],
             addr[4],
             addr[5]);
}

void acfg_set_vap_list(acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile,
        acfg_wlan_profile_vap_list_t *create_list,
        acfg_wlan_profile_vap_list_t *delete_list,
        acfg_wlan_profile_vap_list_t *modify_list)
{
    uint8_t num_new_vap = 0, num_cur_vap = 0;
    acfg_wlan_profile_vap_params_t *vap_param;
    uint8_t vap_matched = 0;

    if (cur_profile == NULL) {
        acfg_log_errstr("%s()- Error !!Current profile cannot be NULL \n\r",__func__);
        return;
    }
    for (num_new_vap = 0; num_new_vap < new_profile->num_vaps;
            num_new_vap++)
    {
        vap_param = &new_profile->vap_params[num_new_vap];
        vap_matched = 0;
        for (num_cur_vap = 0; num_cur_vap < cur_profile->num_vaps;
                num_cur_vap++)
        {
            if (ACFG_STR_MATCH(vap_param->vap_name,
                        cur_profile->vap_params[num_cur_vap].vap_name))
            {
                //put it to modify list
                modify_list->new_vap_idx[modify_list->num_vaps] = num_new_vap;
                modify_list->cur_vap_idx[modify_list->num_vaps] = num_cur_vap;
                modify_list->num_vaps++;
                vap_matched = 1;
                break;
            }
        }
        if (vap_matched == 0) {
            if(vap_param->vap_name[0] == '\0')
                continue;
            //put it to create list
            create_list->new_vap_idx[create_list->num_vaps] = num_new_vap;
            create_list->num_vaps++;
            modify_list->new_vap_idx[modify_list->num_vaps] = num_new_vap;
            modify_list->cur_vap_idx[modify_list->num_vaps] = num_new_vap;
            modify_list->num_vaps++;
        }
    }
    //Check if any vap has to be deleted
    for (num_cur_vap = 0; num_cur_vap < cur_profile->num_vaps;
            num_cur_vap++)
    {
        vap_param = &cur_profile->vap_params[num_cur_vap];
        vap_matched = 0;
        for (num_new_vap = 0; num_new_vap < new_profile->num_vaps;
                num_new_vap++)
        {
            if (ACFG_STR_MATCH(vap_param->vap_name, new_profile->vap_params[num_new_vap].vap_name))
            {
                vap_matched = 1;
                break;
            }

        }
        if (vap_matched == 0) {
            //put it to delete list
            delete_list->cur_vap_idx[delete_list->num_vaps] = num_cur_vap;
            delete_list->num_vaps++;
        }
    }
}

uint32_t
acfg_create_vaps(acfg_wlan_profile_vap_list_t *create_list, acfg_wlan_profile_t *new_profile)
{
    uint8_t i, vap_index;
    acfg_wlan_profile_vap_params_t *vap_profile;
    uint32_t status = QDF_STATUS_SUCCESS;

    /* Create STA vaps first */
    for (i = 0; i < create_list->num_vaps; i++) {
        vap_index = create_list->new_vap_idx[i];
        vap_profile = &new_profile->vap_params[vap_index];
        if(vap_profile->opmode == ACFG_OPMODE_STA)
        {
            status = acfg_wlan_vap_create(vap_profile,
                    new_profile->radio_params);

            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("%s: Failed to create STA VAP (vap=%s status=%d)!\n",
                        __func__, vap_profile->vap_name, status);
                break;
            }
        }
    }
    /* Create AP vaps now */
    for (i = 0; i < create_list->num_vaps; i++) {
        vap_index = create_list->new_vap_idx[i];
        vap_profile = &new_profile->vap_params[vap_index];
        if(vap_profile->opmode != ACFG_OPMODE_STA)
        {
            status = acfg_wlan_vap_create(vap_profile,
                    new_profile->radio_params);

            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("%s: Failed to create AP VAP (vap=%s status=%d)!\n",
                        __func__, vap_profile->vap_name, status);
                break;
            }
        }
    }
    return status;
}

/* Function common to modify and restore operation.
 * (*num_vaps_modified) should be set to a value > 0
 * by caller for restore operation.
 * (*num_vaps_modified) returns num vaps successfully
 * modified on normal modify operation. */

static uint32_t
acfg_iterate_vap_list_and_modify(acfg_wlan_profile_vap_list_t *modify_list,
        acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile,
        acfg_opmode_t opmode,
        uint8_t *num_vaps_modified)
{
    uint32_t restore_status = QDF_STATUS_SUCCESS, status = QDF_STATUS_SUCCESS;
    uint8_t restore_requested = 0;
    uint8_t num_vaps_to_restore = 0;
    uint8_t vap_index;
    uint8_t new_vap_index, cur_vap_index;
    acfg_wlan_profile_vap_params_t *new_vap_profile, *cur_vap_profile;

    if(*num_vaps_modified)
    {
        restore_requested = 1;
        num_vaps_to_restore = *num_vaps_modified;
        *num_vaps_modified = 0;
    }

    for (vap_index = 0; vap_index < modify_list->num_vaps; vap_index++) {
        new_vap_index = modify_list->new_vap_idx[vap_index];
        cur_vap_index = modify_list->cur_vap_idx[vap_index];

        if(restore_requested) {
            new_vap_profile = &cur_profile->vap_params[cur_vap_index];
            cur_vap_profile = &new_profile->vap_params[new_vap_index];
        }
        else {
            new_vap_profile = &new_profile->vap_params[new_vap_index];
            cur_vap_profile = &cur_profile->vap_params[cur_vap_index];
        }

        if(new_vap_profile->opmode == opmode) {
            status = acfg_wlan_vap_profile_modify(new_vap_profile, cur_vap_profile,
                    new_profile->radio_params);
            if (status != QDF_STATUS_SUCCESS)
            {
                acfg_log_errstr("%s: Failed to modify VAP profile (vap=%s status=%d)!\n",
                        __func__, new_vap_profile->vap_name, status);
                if(restore_requested)
                    restore_status = QDF_STATUS_E_FAILURE;
                else
                    return status;
            }

            (*num_vaps_modified)++;

            if((num_vaps_to_restore == (vap_index + 1)) && restore_requested)
            {
                status = restore_status;
                break;
            }
        }
    }

    return status;
}

uint32_t
acfg_modify_profile(acfg_wlan_profile_vap_list_t *modify_list, acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile, int *sec)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    uint8_t num_ap_vaps = 0, num_sta_vaps = 0;

    *sec = 0;

    /* Bring up STA vaps first */
    status = acfg_iterate_vap_list_and_modify(modify_list,
            new_profile,
            cur_profile,
            ACFG_OPMODE_STA,
            &num_sta_vaps);
    if(status != QDF_STATUS_SUCCESS)
    {
        /* There is no need of restore operation here,
         * as this is the first vap modified */
        acfg_log_errstr("%s: Failed to modify STA VAP!\n", __func__);
        return status;
    }

    /* Bring up AP vaps now */
    status = acfg_iterate_vap_list_and_modify(modify_list,
            new_profile,
            cur_profile,
            ACFG_OPMODE_HOSTAP,
            &num_ap_vaps);
    if(status != QDF_STATUS_SUCCESS)
    {
        acfg_log_errstr("%s: Failed to modify AP VAP!\n", __func__);
        /* Restore any VAPs which are modified,
         * This is done to be in sync with config file*/
        if(num_sta_vaps)
        {
            status = acfg_iterate_vap_list_and_modify(modify_list,
                    new_profile,
                    cur_profile,
                    ACFG_OPMODE_STA,
                    &num_sta_vaps);
            if(status !=QDF_STATUS_SUCCESS) {
                acfg_log_errstr("***** Restoring STA vap: failed \n");
            }
            else {
                acfg_log_errstr("***** Restoring STA vap: success\n");
            }
        }
        if(num_ap_vaps)
        {
            status = acfg_iterate_vap_list_and_modify(modify_list,
                    new_profile,
                    cur_profile,
                    ACFG_OPMODE_HOSTAP,
                    &num_ap_vaps);
            if(status !=QDF_STATUS_SUCCESS) {
                acfg_log_errstr("***** Restoring AP vaps: failed \n");
            }
            else {
                acfg_log_errstr("***** Restoring AP vaps: success\n");
            }
        }
        status = QDF_STATUS_E_FAILURE;
        return status;
    }

    *sec = 1;
    return status;
}

uint32_t
acfg_set_vap_profile(acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile,
        acfg_wlan_profile_vap_list_t *create_list,
        acfg_wlan_profile_vap_list_t *delete_list,
        acfg_wlan_profile_vap_list_t *modify_list)
{
    uint8_t i, vap_index;
    uint32_t send_wps_event = 0;
    acfg_wlan_profile_vap_params_t *vap_profile;
    acfg_wlan_profile_vap_params_t *cur_vap_params, *new_vap_params;
    uint32_t status = QDF_STATUS_SUCCESS;
    int sec;

    //Delete Vaps
    for (i = 0; i < delete_list->num_vaps; i++) {
        vap_index = delete_list->cur_vap_idx[i];
        vap_profile = &cur_profile->vap_params[vap_index];
        status = acfg_wlan_vap_profile_delete(vap_profile);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("%s: Failed to delete profile (status=%d)!\n", __func__, status);
            return status;
        }
        new_profile->num_vaps--;
        if (ACFG_IS_VALID_WPS(vap_profile->security_params)) {
            send_wps_event = 1;
        }
        if (ACFG_IS_SEC_ENABLED(vap_profile->security_params.sec_method)) {
            send_wps_event = 1;
        }
    }

    if (cur_profile == NULL) {
        goto done;
    }

    if (cur_profile->num_vaps == delete_list->num_vaps) {
        if (create_list->num_vaps > 0) {
            /*
             * If number of vaps to be deleted is equal to number of current vaps
             * then reset the radio profile params only if there are some vaps to
             * be created.
             */

            acfg_init_radio_params(&cur_profile->radio_params);
            status = acfg_set_radio_profile(&new_profile->radio_params,
                                            &cur_profile->radio_params);
            if (status != QDF_STATUS_SUCCESS) {
                acfg_log_errstr("%s: Failed to set radio profile (status=%d)!\n", __func__, status);
                return status;
            }
        } else {
            /* If the number of vaps to be created is 0, reset the new_profile so
             * that the radio params are reseted next time when the new vap is created.
             */
            acfg_init_radio_params(&new_profile->radio_params);
        }
    }

    //Create vaps
    status = acfg_create_vaps(create_list, new_profile);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to create profile (status=%d)!\n", __func__, status);
        return status;
    }

    for (i = 0; i < create_list->num_vaps; i++) {
        if(i <ACFG_MAX_VAPS){
            vap_index = create_list->new_vap_idx[i];
            cur_vap_params = &cur_profile->vap_params[vap_index];
            new_vap_params = &new_profile->vap_params[vap_index];
            cur_vap_params->opmode = new_vap_params->opmode;
        } else{
            return QDF_STATUS_E_FAILURE;
        }
    }

    if (cur_profile == NULL) {
        goto done;
    }

    //modify vaps
    status = acfg_modify_profile(modify_list, new_profile, cur_profile, &sec);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to create/modify profile (status=%d)!\n", __func__, status);
        return status;
    }
    if (sec == 1) {
        send_wps_event =1;
    }
done:
    if (send_wps_event) {
        acfg_send_interface_event(ACFG_APP_EVENT_INTERFACE_MOD,
                strlen(ACFG_APP_EVENT_INTERFACE_MOD));
    }
    return status;
}

uint32_t acfg_bringup_vaps(acfg_wlan_profile_t *profile,
                             acfg_opmode_t opmode)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    uint8_t i;
    acfg_wlan_profile_vap_params_t *vap_params;

    for (i = 0; i < profile->num_vaps; i++) {
        vap_params = &profile->vap_params[i];
        if ((vap_params->opmode != opmode) ||
                (*vap_params->vap_name == '\0')) {
            continue;
        }
        status = acfg_wlan_iface_up(vap_params->vap_name, vap_params);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("%s: Failed to bring VAP up (vap=%s status=%d)!\n",
                            __func__, vap_params->vap_name, status);
            return QDF_STATUS_E_FAILURE;
        }
    }

    return status;
}

uint32_t acfg_bringdown_vaps(acfg_wlan_profile_t *profile,
        acfg_opmode_t opmode)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    uint8_t i;
    acfg_wlan_profile_vap_params_t *vap_params;

    for (i = 0; i < profile->num_vaps; i++) {
        vap_params = &profile->vap_params[i];
        if ((vap_params->opmode != opmode) ||
                (*vap_params->vap_name == '\0')) {
            continue;
        }
        status = acfg_wlan_iface_down(vap_params->vap_name, vap_params);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("%s: Failed to bring VAP down (vap=%s status=%d)!\n",
                    __func__, vap_params->vap_name, status);
            return QDF_STATUS_E_FAILURE;
        }
    }

    return status;
}

uint32_t acfg_set_radio_profile_chan(acfg_wlan_profile_radio_params_t
        *radio_params,
        acfg_wlan_profile_radio_params_t
        *cur_radio_params,
        acfg_wlan_profile_t *profile,
        acfg_wlan_profile_t *cur_profile)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    uint8_t i;
    acfg_wlan_profile_vap_params_t *vap_params;

    for (i = 0; i < profile->num_vaps; i++) {
        vap_params = &profile->vap_params[i];

        if (vap_params->vap_name[0] == '\0')
            continue;

        status = acfg_set_vap_param(vap_params->vap_name,
                                    ACFG_PARAM_EXT_IFACEUP_ACS,
                                    radio_params->ext_icm);
        if(status != QDF_STATUS_SUCCESS){
            acfg_log_errstr("Failed to en/dis-able external channel manager\n");
            return QDF_STATUS_E_FAILURE;
        }
    }

    if ((radio_params->beacon_burst_mode != cur_radio_params->beacon_burst_mode) ||
         (radio_params->chan != cur_radio_params->chan) ||
         (radio_params->ext_icm != cur_radio_params->ext_icm) ||
         (radio_params->preCAC != cur_radio_params->preCAC) ||
         (profile->num_vaps > 0 && cur_profile->num_vaps == 0)) {
        status = acfg_bringdown_vaps(profile, ACFG_OPMODE_HOSTAP);
        if(status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("%s: Failed to bring AP VAPs down (status=%d)!\n",
                    __func__, status);
            return QDF_STATUS_E_FAILURE;
        }
        for (i = 0; i < profile->num_vaps; i++) {
            vap_params = &profile->vap_params[i];

            if (vap_params->vap_name[0] == '\0')
                continue;

            if (vap_params->opmode == ACFG_OPMODE_STA) {
                continue;
            }
            status = acfg_set_channel(vap_params->vap_name,
                    radio_params->chan);
            if(status != QDF_STATUS_SUCCESS){
                acfg_log_errstr("Failed to set channel\n");
                return QDF_STATUS_E_FAILURE;
            }
        }
    }

    /* First bring STA VAP up */
    status = acfg_bringup_vaps(profile, ACFG_OPMODE_STA);
    if(status != QDF_STATUS_SUCCESS) {
      acfg_log_errstr("%s: Failed to bring STA VAPs up (status=%d)!\n",
                      __func__, status);
      return QDF_STATUS_E_FAILURE;
    }
    /* Bring AP VAPs up */
    status = acfg_bringup_vaps(profile, ACFG_OPMODE_HOSTAP);
    if(status != QDF_STATUS_SUCCESS) {
      acfg_log_errstr("%s: Failed to bring AP VAPs up (status=%d)!\n",
                      __func__, status);
      return QDF_STATUS_E_FAILURE;

    }

    return status;
}

uint32_t acfg_set_radio_profile(acfg_wlan_profile_radio_params_t
        *radio_params,
        acfg_wlan_profile_radio_params_t
        *cur_radio_params)
{
    uint32_t status = QDF_STATUS_SUCCESS;

    status = acfg_wlan_iface_present((char *)radio_params->radio_name);
    if(status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("Radio not present\n");
        return QDF_STATUS_E_INVAL;
    }
    if (!ACFG_STR_MATCH((char *)radio_params->radio_mac,
                (char *)cur_radio_params->radio_mac)) {
        status = acfg_set_ifmac ((char *)radio_params->radio_name,
                (char *)radio_params->radio_mac,
                ARPHRD_IEEE80211);
        if (status != QDF_STATUS_SUCCESS) {
            //return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->country_code != cur_radio_params->country_code) {
        if(radio_params->country_code != 0) {
            status = acfg_set_radio_param(radio_params->radio_name,
                    ACFG_PARAM_RADIO_COUNTRYID,
                    radio_params->country_code);
            if (status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
    if (radio_params->ampdu != cur_radio_params->ampdu) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_AMPDU,
                !!radio_params->ampdu);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->ampdu_limit_bytes !=
            cur_radio_params->ampdu_limit_bytes)
    {
        if (radio_params->ampdu_limit_bytes) {
            status = acfg_set_radio_param(radio_params->radio_name,
                    ACFG_PARAM_RADIO_AMPDU_LIMIT,
                    radio_params->ampdu_limit_bytes);
            if (status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
        else {
            acfg_log_errstr("Invalid value for ampdu limit \n\r");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->ampdu_subframes != cur_radio_params->ampdu_subframes)
    {
        if (radio_params->ampdu_subframes) {
            status = acfg_set_radio_param(radio_params->radio_name,
                    ACFG_PARAM_RADIO_AMPDU_SUBFRAMES,
                    radio_params->ampdu_subframes);
            if (status != QDF_STATUS_SUCCESS) {
                return QDF_STATUS_E_FAILURE;
            }
        }
        else {
            acfg_log_errstr("Invalid value for ampdu subframes \n\r");
            return QDF_STATUS_E_FAILURE;
        }
    }
    if(radio_params->macreq_enabled != cur_radio_params->macreq_enabled)
    {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_ENABLE_MAC_REQ,
                radio_params->macreq_enabled);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->aggr_burst != cur_radio_params->aggr_burst) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_AGGR_BURST,
                radio_params->aggr_burst);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->aggr_burst_dur != cur_radio_params->aggr_burst_dur) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_AGGR_BURST_DUR,
                radio_params->aggr_burst_dur);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->ccathena != cur_radio_params->ccathena) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_HAL_CONFIG_ENABLEADAPTIVECCATHRES,
                radio_params->ccathena);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->cca_det_level != cur_radio_params->cca_det_level) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_HAL_CONFIG_CCA_DETECTION_LEVEL,
                radio_params->cca_det_level);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->cca_det_margin != cur_radio_params->cca_det_margin) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_HAL_CONFIG_CCA_DETECTION_MARGIN,
                radio_params->cca_det_margin);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->beacon_burst_mode != cur_radio_params->beacon_burst_mode) {
        status = acfg_set_radio_param( radio_params->radio_name,
                ACFG_PARAM_RADIO_BEACON_BURST_MODE,
                radio_params->beacon_burst_mode );
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->sta_dfs_enable != cur_radio_params->sta_dfs_enable) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_STADFS_ENABLE,
                radio_params->sta_dfs_enable);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->atf_strict_sched != cur_radio_params->atf_strict_sched) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_ATF_STRICT_SCHED,
                !!(radio_params->atf_strict_sched & ACFG_FLAG_ATF_STRICT_SCHED));
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_ATF_OBSS_SCHED,
                !!(radio_params->atf_strict_sched & ACFG_FLAG_ATF_OBSS_SCHED));
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->preCAC != cur_radio_params->preCAC) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_PRECAC_ENABLE,
                radio_params->preCAC);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->interCACChan != cur_radio_params->interCACChan) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_PRECAC_INTER_CHANNEL,
                radio_params->interCACChan);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (radio_params->preCACtimeout != cur_radio_params->preCACtimeout) {
        status = acfg_set_radio_param(radio_params->radio_name,
                ACFG_PARAM_RADIO_PRECAC_TIMEOUT,
                radio_params->preCACtimeout);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }

    return status;
}

void
acfg_init_profile(acfg_wlan_profile_t *profile)
{
    acfg_wlan_profile_radio_params_t *radio_params;
    acfg_wlan_profile_vap_params_t *vap_params;
    int i;
    radio_params = &profile->radio_params;
    acfg_init_radio_params (radio_params);
    for(i = 0; i < ACFG_MAX_VAPS; i++) {
        vap_params = &profile->vap_params[i];
        acfg_init_vap_params (vap_params);
   }
}

void
acfg_init_radio_params (acfg_wlan_profile_radio_params_t *unspec_radio_params)
{
    unspec_radio_params->chan = 0;
    unspec_radio_params->freq = 0;
    unspec_radio_params->country_code = 0;
    memset(unspec_radio_params->radio_mac, 0, ACFG_MACSTR_LEN);
    unspec_radio_params->macreq_enabled = 0xff;
    unspec_radio_params->ampdu = -1;
    unspec_radio_params->ampdu_limit_bytes = 0;
    unspec_radio_params->ampdu_subframes = 0;
    unspec_radio_params->aggr_burst = -1;
    unspec_radio_params->aggr_burst_dur = 0;
    unspec_radio_params->sta_dfs_enable = 0;
    unspec_radio_params->atf_strict_sched = -1;
    unspec_radio_params->ccathena = 0;
    unspec_radio_params->cca_det_level = -70;
    unspec_radio_params->cca_det_margin = 3;
    unspec_radio_params->beacon_burst_mode = 0;/* default value 0 */
    unspec_radio_params->preCAC = 0;
    unspec_radio_params->interCACChan = 0;
    unspec_radio_params->preCACtimeout = 60;
}

void
acfg_init_vap_params (acfg_wlan_profile_vap_params_t *unspec_vap_params)
{
    acfg_wlan_profile_node_params_t *unspec_node_params;
    acfg_wds_params_t *unspec_wds_params;
    acfg_wlan_profile_security_params_t *unspec_security_params;
    acfg_wlan_profile_sec_eap_params_t *unspec_eap_params;
    acfg_wlan_profile_sec_radius_params_t *unspec_radius_params;
    acfg_wlan_profile_sec_acct_server_params_t *unspec_acct_params;
    acfg_wlan_profile_sec_hs_iw_param_t *unspec_hs_params;
    int j;
    memset(unspec_vap_params->vap_name, 0, ACFG_MAX_IFNAME);
    unspec_vap_params->vap_name[0]='\0';
    memset(unspec_vap_params->radio_name, 0, ACFG_MAX_IFNAME);
    unspec_vap_params->opmode = ACFG_OPMODE_INVALID;
    unspec_vap_params->vapid = 0xffffffff;
    unspec_vap_params->phymode = ACFG_PHYMODE_INVALID;
    unspec_vap_params->ampdu = -1;
    memset(unspec_vap_params->ssid, 0, (ACFG_MAX_SSID_LEN + 1));
    unspec_vap_params->bitrate = -1;
    for(j = 0; j < 16; j++)
        unspec_vap_params->rate[0] = -1;
    unspec_vap_params->retries = -1;
    unspec_vap_params->txpow = -1;
    unspec_vap_params->beacon_interval = 0;
    unspec_vap_params->dtim_period = 0;
    unspec_vap_params->atf_options = -1;
    unspec_vap_params->rts_thresh = ACFG_RTS_INVALID;
    unspec_vap_params->frag_thresh = ACFG_FRAG_INVALID;
    memset(unspec_vap_params->vap_mac, 0, ACFG_MACSTR_LEN);
    unspec_node_params = &unspec_vap_params->node_params;
    for(j = 0; j < ACFG_MAX_ACL_NODE; j++) {
        memset(unspec_node_params->acfg_acl_node_list[j], 0, ACFG_MACADDR_LEN);
    }
    unspec_node_params->num_node = 0;
    unspec_node_params->node_acl = ACFG_WLAN_PROFILE_NODE_ACL_INVALID;

    for(j = 0; j < ACFG_MAX_ACL_NODE; j++) {
        memset(unspec_node_params->acfg_acl_node_list_sec[j], 0, ACFG_MACADDR_LEN);
    }
    unspec_node_params->num_node_sec = 0;
    unspec_node_params->node_acl_sec = ACFG_WLAN_PROFILE_NODE_ACL_INVALID;

    unspec_wds_params = &unspec_vap_params->wds_params;
    unspec_wds_params->enabled = -1;
    memset(unspec_wds_params->wds_addr, 0, ACFG_MACSTR_LEN);
    unspec_wds_params->wds_flags = ACFG_FLAG_INVALID;
    unspec_vap_params->vlanid = ACFG_WLAN_PROFILE_VLAN_INVALID;
    memset(unspec_vap_params->bridge, 0 , ACFG_MAX_IFNAME);
    unspec_vap_params->pureg = -1;
    unspec_vap_params->puren = -1;
    unspec_vap_params->hide_ssid = -1;
    unspec_vap_params->doth = -1;
    unspec_vap_params->client_isolation = -1;
    unspec_vap_params->coext = -1;
    unspec_vap_params->uapsd = -1;
    unspec_vap_params->shortgi = -1;
    unspec_vap_params->amsdu = -1;
    unspec_vap_params->max_clients = 0;
    unspec_security_params = &unspec_vap_params->security_params;
    unspec_security_params->sec_method = ACFG_WLAN_PROFILE_SEC_METH_INVALID;
    unspec_security_params->cipher_method = ACFG_WLAN_PROFILE_CIPHER_METH_INVALID;
    unspec_security_params->g_cipher_method = ACFG_WLAN_PROFILE_CIPHER_METH_INVALID;
    unspec_security_params->sha256 = 0;
    unspec_security_params->ieee80211w = 0;
    unspec_security_params->group_mgmt_cipher = ACFG_WLAN_PROFILE_GRP_MGMT_CIPHER_INVALID;
    unspec_security_params->assoc_sa_query_max_timeout = 0;
    unspec_security_params->assoc_sa_query_retry_timeout = 0;
    memset(unspec_security_params->psk, 0, ACFG_MAX_PSK_LEN);
    memset(unspec_security_params->wep_key0, 0, ACFG_MAX_WEP_KEY_LEN);
    memset(unspec_security_params->wep_key1, 0, ACFG_MAX_WEP_KEY_LEN);
    memset(unspec_security_params->wep_key2, 0, ACFG_MAX_WEP_KEY_LEN);
    memset(unspec_security_params->wep_key3, 0, ACFG_MAX_WEP_KEY_LEN);
    unspec_security_params->wep_key_defidx = 0;
    unspec_security_params->wps_pin = 0;
    unspec_security_params->wps_flag = 0;
    memset(unspec_security_params->wps_manufacturer, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_model_name, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_model_number, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_serial_number, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_device_type, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_config_methods, 0, ACFG_WPS_CONFIG_METHODS_LEN);
    memset(unspec_security_params->wps_upnp_iface, 0, ACFG_MAX_IFNAME);
    memset(unspec_security_params->wps_friendly_name, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_man_url, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_model_desc, 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_upc, 0, ACFG_WSUPP_PARAM_LEN);
    unspec_security_params->wps_pbc_in_m1 = 0;
    memset(unspec_security_params->wps_device_name , 0, ACFG_WSUPP_PARAM_LEN);
    memset(unspec_security_params->wps_rf_bands, 0, ACFG_WPS_RF_BANDS_LEN);
    unspec_eap_params = &unspec_security_params->eap_param;
    unspec_eap_params->eap_type = 0;
    memset(unspec_eap_params->identity , 0, EAP_IDENTITY_LEN);
    memset(unspec_eap_params->password , 0, EAP_PASSWD_LEN);
    memset(unspec_eap_params->ca_cert , 0, EAP_FILE_NAME_LEN);
    memset(unspec_eap_params->client_cert , 0, EAP_FILE_NAME_LEN);
    memset(unspec_eap_params->private_key , 0, EAP_FILE_NAME_LEN);
    memset(unspec_eap_params->private_key_passwd , 0, EAP_PVT_KEY_PASSWD_LEN);
    unspec_security_params->radius_retry_primary_interval = 0;
    unspec_radius_params = &unspec_security_params->pri_radius_param;
    memset(unspec_radius_params->radius_ip, 0, IP_ADDR_LEN);
    unspec_radius_params->radius_port = 0;
    memset(unspec_radius_params->shared_secret, 0 , RADIUS_SHARED_SECRET_LEN);
    unspec_radius_params = &unspec_security_params->sec1_radius_param;
    memset(unspec_radius_params->radius_ip, 0, IP_ADDR_LEN);
    unspec_radius_params->radius_port = 0;
    memset(unspec_radius_params->shared_secret, 0 , RADIUS_SHARED_SECRET_LEN);
    unspec_radius_params = &unspec_security_params->sec2_radius_param;
    memset(unspec_radius_params->radius_ip, 0, IP_ADDR_LEN);
    unspec_radius_params->radius_port = 0;
    memset(unspec_radius_params->shared_secret, 0 , RADIUS_SHARED_SECRET_LEN);
    unspec_acct_params = &unspec_security_params->pri_acct_server_param;
    memset(unspec_acct_params->acct_ip, 0, IP_ADDR_LEN);
    unspec_acct_params->acct_port = 0;
    memset(unspec_acct_params->shared_secret, 0 , ACCT_SHARED_SECRET_LEN);
    unspec_acct_params = &unspec_security_params->sec1_acct_server_param;
    memset(unspec_acct_params->acct_ip, 0, IP_ADDR_LEN);
    unspec_acct_params->acct_port = 0;
    memset(unspec_acct_params->shared_secret, 0 , ACCT_SHARED_SECRET_LEN);
    unspec_acct_params = &unspec_security_params->sec2_acct_server_param;
    memset(unspec_acct_params->acct_ip, 0, IP_ADDR_LEN);
    unspec_acct_params->acct_port = 0;
    memset(unspec_acct_params->shared_secret, 0 , ACCT_SHARED_SECRET_LEN);
    unspec_hs_params = &unspec_security_params->hs_iw_param;
    unspec_hs_params->hs_enabled = 0;
    unspec_hs_params->iw_enabled = 0;
    unspec_vap_params->primary_vap = 0;
    unspec_vap_params->bcn_rate = 0;
    unspec_vap_params->implicitbf = -1;
    unspec_vap_params->rrm = -1;
    unspec_vap_params->wnm = -1;
}

static uint32_t acfg_alloc_profile(acfg_wlan_profile_t **new_profile, acfg_wlan_profile_t **curr_profile)
{
    *new_profile = *curr_profile = NULL;

    *new_profile = malloc(sizeof(acfg_wlan_profile_t));
    if (*new_profile == NULL) {
        acfg_log_errstr("%s: mem alloc failure\n", __FUNCTION__);
        return QDF_STATUS_E_FAILURE;
    }
    *curr_profile = malloc(sizeof(acfg_wlan_profile_t));
    if (*curr_profile == NULL) {
        acfg_log_errstr("%s: mem alloc failure\n", __FUNCTION__);
        free(*new_profile);
        *new_profile = NULL;
        return QDF_STATUS_E_FAILURE;
    }
    memset(*new_profile, 0, sizeof(acfg_wlan_profile_t));
    memset(*curr_profile, 0, sizeof(acfg_wlan_profile_t));

    return QDF_STATUS_SUCCESS;
}

static uint32_t acfg_populate_profile(acfg_wlan_profile_t *curr_profile, char *radioname)
{
    char curr_profile_file[64];
    FILE *fp;
    int ret = 0;

    snprintf(curr_profile_file, sizeof(curr_profile_file),
                "/etc/acfg_curr_profile_%s.conf.bin", radioname);
    fp =  fopen (curr_profile_file,"rb");
    if(fp == NULL) {
        acfg_log_errstr(" %s not found. Initializing profile \n\r",curr_profile_file);
        return QDF_STATUS_E_INVAL;
    } else {
        ret = fread(curr_profile ,1,sizeof(acfg_wlan_profile_t),fp);
        if(!ret) {
            acfg_log_errstr("ERROR !! %s could not be read!!\n\r",curr_profile_file);
            fclose(fp);
            return QDF_STATUS_E_FAILURE;
        }
        fclose(fp);
    }
    return QDF_STATUS_SUCCESS;
}

acfg_wlan_profile_t * acfg_get_profile(char *radioname)
{
    acfg_wlan_profile_t *new_profile, *curr_profile;

    acfg_reset_errstr();

    if (acfg_alloc_profile(&new_profile, &curr_profile) != QDF_STATUS_SUCCESS)
        return NULL;

    if (acfg_populate_profile(curr_profile, radioname) != QDF_STATUS_SUCCESS) {
        acfg_init_profile(curr_profile);
    }

    memcpy(new_profile, curr_profile, sizeof(acfg_wlan_profile_t));
    new_profile->priv = (void*)curr_profile;
    return new_profile;
}

uint32_t acfg_recover_profile(char *radioname)
{
    acfg_wlan_profile_t *new_profile, *curr_profile;
    int status = QDF_STATUS_SUCCESS;
    int i;
    int ret = 0;

    acfg_reset_errstr();

    if (acfg_alloc_profile(&new_profile, &curr_profile) != QDF_STATUS_SUCCESS)
        return QDF_STATUS_E_FAILURE;

    if (acfg_populate_profile(new_profile, radioname) == QDF_STATUS_E_INVAL) {
        /* no ACFG config found, try uci */
        ret = system("/sbin/wifi recover");
        return ret;
    }

    acfg_init_profile(curr_profile);

    for (i = 0; i < new_profile->num_vaps; i++) {
        strlcpy((char *)curr_profile->vap_params[i].vap_name,
                (char *)new_profile->vap_params[i].vap_name, sizeof(curr_profile->vap_params[i].vap_name));
        curr_profile->vap_params[i].opmode = new_profile->vap_params[i].opmode;
    }
    curr_profile->num_vaps = new_profile->num_vaps;

    new_profile->priv = (void*)curr_profile;

    /* Apply the new profile */
    status = acfg_apply_profile(new_profile);

    /* Free cur_profile & new_profile */
    acfg_free_profile(new_profile);

    return status;
}

void acfg_free_profile(acfg_wlan_profile_t * profile)
{
    free(profile->priv);
    free(profile);
}

uint32_t
acfg_set_profile(acfg_wlan_profile_t *new_profile,
        acfg_wlan_profile_t *cur_profile)
{
    uint32_t   status = QDF_STATUS_SUCCESS;
    acfg_wlan_profile_vap_list_t create_list, modify_list, delete_list;

    memset(&create_list, 0, sizeof (acfg_wlan_profile_vap_list_t));
    memset(&delete_list, 0, sizeof (acfg_wlan_profile_vap_list_t));
    memset(&modify_list, 0, sizeof (acfg_wlan_profile_vap_list_t));

    acfg_set_vap_list(new_profile, cur_profile,
            &create_list, &delete_list,
            &modify_list);
    if (cur_profile == NULL) {
        return QDF_STATUS_E_FAILURE;
    } else {
        status = acfg_set_radio_profile(&new_profile->radio_params,
                &cur_profile->radio_params);
    }
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Failed to set radio profile (radio=%s status=%d)!\n",
                __func__, new_profile->radio_params.radio_name, status);
        return status;
    }
   if(cur_profile != NULL) {
       status = acfg_set_vap_profile(new_profile, cur_profile,
         	   &create_list, &delete_list,
		   &modify_list);
       if (status != QDF_STATUS_SUCCESS) {
	   acfg_log_errstr("%s: Failed to set VAP profile (vap=%s status=%d)!\n",
			   __func__, new_profile->vap_params->vap_name, status);
	   return status;
       }
    }

    if (cur_profile != NULL) {
        status = acfg_set_radio_profile_chan(&new_profile->radio_params,
                &cur_profile->radio_params,
                new_profile, cur_profile);
        if (status != QDF_STATUS_SUCCESS) {
            acfg_log_errstr("%s: Failed to set radio profile channel (vap=%s status=%d)!\n",
                    __func__, new_profile->radio_params.radio_name, status);
            return status;
        }
    }

    return status;
}

uint32_t
acfg_write_file(acfg_wlan_profile_t *new_profile)
{
    int i=0, ret=0;
    FILE *fp;
    char curr_profile_file[64];
    uint32_t status = QDF_STATUS_SUCCESS;

    snprintf(curr_profile_file, sizeof(curr_profile_file),
             "/etc/acfg_curr_profile_%s.conf.bin",
             new_profile->radio_params.radio_name);
    fp =  fopen (curr_profile_file,"wb");
    if(fp != NULL) {
        int valid_vaps = 0;

        acfg_print("%s: INFO: '%s' VAP cnt is %u\n", __func__, new_profile->radio_params.radio_name, new_profile->num_vaps);
        /* move valid VAPs to the front, clear all other */
        for(i=0; i < ACFG_MAX_VAPS; i++)
        {
            if (new_profile->vap_params[i].vap_name[0] == '\0')
            {
                acfg_print("%s: INFO: '%s' clearing VAP index %d\n", __func__, new_profile->radio_params.radio_name, i);
                acfg_init_vap_params(&new_profile->vap_params[i]);
                continue;
            }
            acfg_print("%s: INFO: '%s' valid VAP index %d\n", __func__, new_profile->radio_params.radio_name, i);
            if (i > valid_vaps)
            {
                acfg_print("%s: INFO: '%s' moving VAP '%s' to index %d\n", __func__,
                    new_profile->radio_params.radio_name, new_profile->vap_params[i].vap_name, valid_vaps);
                memcpy(&new_profile->vap_params[valid_vaps], &new_profile->vap_params[i], sizeof(acfg_wlan_profile_vap_params_t));
                acfg_init_vap_params(&new_profile->vap_params[i]);
            }
            valid_vaps++;
        }
        acfg_print("%s: INFO: '%s' VAP cnt is %u\n", __func__, new_profile->radio_params.radio_name, new_profile->num_vaps);
        ret = fwrite(new_profile, 1, sizeof(acfg_wlan_profile_t), fp);
        if(!ret)
            status = QDF_STATUS_E_FAILURE;
        fclose(fp);
    } else {
        acfg_log_errstr("%s could not be opened for writing \n\r",curr_profile_file);
        status = QDF_STATUS_E_FAILURE;
    }
    return status;
}

uint32_t
acfg_reset_cur_profile(char *radio_name)
{
    char curr_profile_file[64];
    uint32_t status = QDF_STATUS_SUCCESS;

    snprintf(curr_profile_file, sizeof(curr_profile_file),
             "/etc/acfg_curr_profile_%s.conf.bin", radio_name);

    /* Do not return failure, if current profile file doesn't exist */
    errno = 0;
    if((unlink(curr_profile_file) < 0) && (errno != ENOENT)) {
        status = QDF_STATUS_E_FAILURE;
    }

    return status;
}

uint32_t
acfg_apply_profile(acfg_wlan_profile_t *new_profile)
{
    uint32_t  status = QDF_STATUS_SUCCESS;
    acfg_wlan_profile_t * curr_profile = NULL;

    if (new_profile == NULL)
        return QDF_STATUS_E_FAILURE;

    curr_profile = (acfg_wlan_profile_t *)new_profile->priv;
    status = acfg_set_profile(new_profile, curr_profile);
    if (status == QDF_STATUS_SUCCESS) {
        acfg_write_file(new_profile);
    }
    return status;
}

uint32_t acfg_set_radio_enable(uint8_t *ifname)
{
    return acfg_wlan_iface_up(ifname, NULL);
}

uint32_t acfg_set_radio_disable(uint8_t *ifname)
{
    return acfg_wlan_iface_down(ifname, NULL);
}

uint32_t acfg_set_country(uint8_t *radio_name, uint16_t country_code)
{
    uint32_t   status = QDF_STATUS_SUCCESS;

    if(country_code == 0) {
        return QDF_STATUS_E_FAILURE;
    }

    status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_COUNTRYID,
                                  country_code);

    return status;
}

uint32_t
acfg_get_country(uint8_t *radio_name, uint32_t *country_code)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t val;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_get_radio_param(radio_name, ACFG_PARAM_RADIO_COUNTRYID, &val);

    if (status == QDF_STATUS_SUCCESS) {
        *country_code = val;
    }

    return status;
}

uint32_t
acfg_get_regdomain(uint8_t *radio_name, uint32_t *regdomain)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t val;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_get_radio_param(radio_name, ACFG_PARAM_RADIO_REGDOMAIN, &val);

    if (status == QDF_STATUS_SUCCESS) {
        *regdomain = val;
    }

    return status;
}

uint32_t acfg_set_shpreamble(uint8_t *radio_name, uint16_t shpreamble)
{
    uint32_t   status = QDF_STATUS_SUCCESS;

    status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_ENABLE_SHPREAMBLE,
                                  shpreamble);

    return status;
}

uint32_t
acfg_get_shpreamble(uint8_t *radio_name, uint32_t *shpreamble)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t val;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_get_radio_param(radio_name, ACFG_PARAM_RADIO_ENABLE_SHPREAMBLE, &val);

    if (status == QDF_STATUS_SUCCESS) {
        *shpreamble = val;
    }

    return status;
}

uint32_t
acfg_set_shslot(uint8_t *radio_name, uint16_t shslot)
{
    uint32_t   status = QDF_STATUS_SUCCESS;

    status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_ENABLE_SHSLOT,
                                  shslot);

    return status;
}

uint32_t
acfg_get_shslot(uint8_t *radio_name, uint32_t *shslot)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t val;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_get_radio_param(radio_name, ACFG_PARAM_RADIO_ENABLE_SHSLOT, &val);

    if (status == QDF_STATUS_SUCCESS) {
        *shslot = val;
    }

    return status;
}

uint32_t
acfg_set_txpower_limit(uint8_t *radio_name, enum acfg_band_type band, uint32_t power)
{
    uint32_t   status = QDF_STATUS_SUCCESS;

    switch (band) {
    case ACFG_BAND_2GHZ:
        status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_TXPOWER_LIMIT2G, power);
        break;
    case ACFG_BAND_5GHZ:
        status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_TXPOWER_LIMIT5G, power);
        break;
    default:
        break;
    }

    return status;
}

void
acfg_get_wep_str(char *str, uint8_t *key, uint8_t key_len, uint16_t str_max_len)
{
    if (key_len == 5) {
        snprintf(str, str_max_len, "%02x%02x%02x%02x%02x", key[0],
                 key[1],
                 key[2],
                 key[3],
                 key[4]);
    }
    if (key_len == 13) {
        snprintf(str, str_max_len,
                 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 key[0], key[1], key[2],
                 key[3], key[4], key[5],
                 key[6], key[7], key[8],
                 key[9], key[10], key[11],
                 key[12]);
    }
    if (key_len == 16) {
        snprintf(str, str_max_len,
                 "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                 key[0], key[1], key[2],
                 key[3], key[4], key[5],
                 key[6], key[7], key[8],
                 key[9], key[10], key[11],
                 key[12], key[13], key[14],
                 key[15]);
    }
}


uint32_t
acfg_get_vap_info(uint8_t *ifname,
        acfg_wlan_profile_vap_params_t *vap_params)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_macacl_t maclist;
    uint8_t i = 0;
    uint32_t val;
    acfg_ssid_t ssid;

    status = acfg_get_opmode(ifname, &vap_params->opmode);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    status = acfg_get_ssid(ifname, &ssid);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    acfg_os_strcpy((uint8_t *)vap_params->ssid, (uint8_t *)ssid.ssid, sizeof(vap_params->ssid));

    status = acfg_get_rate(ifname, &vap_params->bitrate);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    vap_params->bitrate = vap_params->bitrate / 1000000;

    status = acfg_get_txpow(ifname, &vap_params->txpow);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_rssi(ifname, &vap_params->rssi);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_rts(ifname, &vap_params->rts_thresh);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_frag(ifname, &vap_params->frag_thresh);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_acl_getmac(ifname, &maclist);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_acl_getmac_secondary(ifname, &maclist);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_MACCMD,
            &vap_params->node_params.node_acl);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    for (i = 0; i < maclist.num; i++) {
        memcpy(vap_params->node_params.acfg_acl_node_list[i],
                maclist.macaddr[i], ACFG_MACADDR_LEN);
    }
    vap_params->node_params.num_node = maclist.num;

    /* For Seconnd ACL list*/
    status = acfg_get_vap_param(ifname, ACFG_PARAM_MACCMD_SEC,
            &vap_params->node_params.node_acl_sec);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    for (i = 0; i < maclist.num; i++) {
        memcpy(vap_params->node_params.acfg_acl_node_list_sec[i],
                maclist.macaddr[i], ACFG_MACADDR_LEN);
    }
    vap_params->node_params.num_node_sec = maclist.num;

    status = acfg_get_vap_param(ifname, ACFG_PARAM_VAP_SHORT_GI, &val);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_VAP_AMPDU, &val);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    vap_params->ampdu = !!(val); /* double negation */

    status = acfg_get_vap_param(ifname, ACFG_PARAM_HIDESSID, &vap_params->hide_ssid);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_APBRIDGE, &val);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    vap_params->client_isolation = !val;

    status = acfg_get_vap_param(ifname, ACFG_PARAM_BEACON_INTERVAL,
            &vap_params->beacon_interval);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_PUREG, &vap_params->pureg);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_UAPSD, &vap_params->uapsd);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_PUREN, &vap_params->puren);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_EXTAP, &val);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    if (val) {
        vap_params->wds_params.wds_flags |= ACFG_FLAG_EXTAP;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_DISABLECOEXT, &val);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    vap_params->coext = !!(val);

    status = acfg_get_vap_param(ifname, ACFG_PARAM_DOTH, &vap_params->doth);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }

    status = acfg_get_vap_param(ifname, ACFG_PARAM_MODIFY_BEACON_RATE, &val);
    if(status != QDF_STATUS_SUCCESS){
        return status;
    }
    vap_params->bcn_rate = val;

    status = acfg_get_vap_param(ifname, ACFG_PARAM_IMPLICITBF, &val);
    if(status != QDF_STATUS_SUCCESS) {
        return status;
    }
    vap_params->implicitbf = val;

    status = acfg_get_vap_param(ifname, ACFG_PARAM_WNM_ENABLE, &val);
    if(status != QDF_STATUS_SUCCESS) {
        return status;
    }
    vap_params->wnm = val;

    status = acfg_get_vap_param(ifname, ACFG_PARAM_RRM_CAP, &val);
    if(status != QDF_STATUS_SUCCESS) {
        return status;
    }
    vap_params->rrm = val;

    return status;
}

uint32_t
acfg_get_current_profile(acfg_wlan_profile_t *profile)
{
    uint32_t status = QDF_STATUS_E_FAILURE, final_status = QDF_STATUS_SUCCESS;
    acfg_os_req_t	req = {.cmd = ACFG_REQ_GET_PROFILE};
    acfg_radio_vap_info_t *ptr;
    int i = 0;

    ptr = (acfg_radio_vap_info_t *)req.data;
    if (acfg_os_check_str(profile->radio_params.radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_os_send_req(profile->radio_params.radio_name, &req);
    if (status == QDF_STATUS_SUCCESS) {
        acfg_os_strcpy((uint8_t *)profile->radio_params.radio_name, (uint8_t *)ptr->radio_name, ACFG_MAX_IFNAME);
        memcpy(profile->radio_params.radio_mac,
                ptr->radio_mac,
                ACFG_MACADDR_LEN);
        profile->radio_params.chan = ptr->chan;
        profile->radio_params.freq = ptr->freq;
        profile->radio_params.country_code = ptr->country_code;

        for (i = 0; i <  ptr->num_vaps; i++) {
            status = acfg_get_vap_info(ptr->vap_info[i].vap_name,
                    &profile->vap_params[i]);
            if(status != QDF_STATUS_SUCCESS){
                acfg_log_errstr("%s: Get vap info failed for %s\n", __func__,
                        ptr->vap_info[i].vap_name);
                final_status = QDF_STATUS_E_FAILURE;
                continue;
            }
            acfg_os_strcpy((uint8_t *)profile->vap_params[i].vap_name,
                           (uint8_t *)ptr->vap_info[i].vap_name,
                            sizeof(profile->vap_params[i].vap_name));
            memcpy(profile->vap_params[i].vap_mac,
                    ptr->vap_info[i].vap_mac,
                    ACFG_MACADDR_LEN);
            profile->vap_params[i].phymode = ptr->vap_info[i].phymode;
            profile->vap_params[i].security_params.sec_method =
                ptr->vap_info[i].sec_method;
            profile->vap_params[i].security_params.cipher_method =
                ptr->vap_info[i].cipher;
            if (ptr->vap_info[i].wep_key_len) {
                if (ptr->vap_info[i].wep_key_idx == 0) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key0,
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len, ACFG_MAX_WEP_KEY_LEN);
                } else if (ptr->vap_info[i].wep_key_idx == 1) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key1,
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len, ACFG_MAX_WEP_KEY_LEN);
                } else if (ptr->vap_info[i].wep_key_idx == 2) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key2,
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len, ACFG_MAX_WEP_KEY_LEN);
                } else if (ptr->vap_info[i].wep_key_idx == 3) {
                    acfg_get_wep_str(profile->vap_params[i].security_params.wep_key3,
                            ptr->vap_info[i].wep_key,
                            ptr->vap_info[i].wep_key_len, ACFG_MAX_WEP_KEY_LEN);
                }
            }
        }
        profile->num_vaps = ptr->num_vaps;
    } else {
        acfg_log_errstr("%s: Error sending cmd\n", __func__);
    }
    return final_status;
}

void
acfg_fill_wps_config(char *ifname, char *buf)
{
    char filename[32];
    FILE *fp;

    snprintf(filename, sizeof(filename), "%s_%s.conf",
            ACFG_WPS_CONFIG_PREFIX, ifname);
    fp = fopen(filename, "w");
    if(fp != NULL){
    	fprintf(fp, "%s", buf);
        fclose(fp);
    }

}

uint32_t
acfg_get_wps_cred(uint8_t *ifname, acfg_opmode_t opmode,
        char *buffer, int *buflen)
{
    char cmd[255], replybuf[4096];
    uint32_t len;

    len = sizeof(replybuf);

    memset(cmd, '\0', sizeof(cmd));
    memset(replybuf, '\0', sizeof(replybuf));
    snprintf(cmd, sizeof(cmd), "%s", "WPS_GET_CONFIG");
    if(acfg_ctrl_req(ifname, cmd, strlen(cmd),
                replybuf, &len, opmode) < 0){
        acfg_log_errstr("%s: cmd --> %s failed for %s\n", __func__,
                cmd,
                ifname);
        return QDF_STATUS_E_FAILURE;
    }
    *buflen = len;
    acfg_os_strcpy((uint8_t *)buffer, (uint8_t *)replybuf, ACFG_MAX_WPS_FILE_SIZE);

    return QDF_STATUS_SUCCESS;
}


void
acfg_parse_cipher(char *value, acfg_wps_cred_t *wps_cred)
{
    int last;
    char *start, *end, buf[255];

    snprintf(buf, sizeof(buf), "%s", value);
    start = buf;
    while (*start != '\0') {
        while (*start == ' ' || *start == '\t')
            start++;
        if (*start == '\0')
            break;
        end = start;
        while (*end != ' ' && *end != '\t' && *end != '\0' && *end != '\n')
            end++;
        last = *end == '\0';
        *end = '\0';
        if (strcmp(start, "CCMP") == 0) {
            wps_cred->enc_type |=
                ACFG_WLAN_PROFILE_CIPHER_METH_AES;
        }
        else if (strcmp(start, "TKIP") == 0) {
            wps_cred->enc_type |=
                ACFG_WLAN_PROFILE_CIPHER_METH_TKIP;
        }
        if (last) {
            break;
        }
        start = end + 1;
    }
}

uint32_t
acfg_parse_wpa_key_mgmt(char *value,
        acfg_wps_cred_t *wps_cred)
{
    int last;
    char *start, *end, *buf;

    buf = strdup(value);
    if (buf == NULL)
        return QDF_STATUS_E_FAILURE;
    start = buf;
    while (*start != '\0') {
        while (*start == ' ' || *start == '\t')
            start++;
        if (*start == '\0')
            break;
        end = start;
        while (*end != ' ' && *end != '\t' && *end != '\0' && *end != '\n')
            end++;
        last = *end == '\0';
        *end = '\0';
        if (strcmp(start, "WPA-PSK") == 0) {
            wps_cred->key_mgmt = 2;
        }
        else if (strcmp(start, "WPA-EAP") == 0) {
            wps_cred->key_mgmt = 1;
        }
        if (last) {
            break;
        }
        start = end + 1;
    }
    free(buf);
    return QDF_STATUS_SUCCESS;
}

int
acfg_get_wps_config(uint8_t *ifname, acfg_wps_cred_t *wps_cred)
{
    char filename[32];
    FILE *fp;
    char *pos;
    int val = 0, ret = 1, len = 0, buflen = 0;
    char buf[255];

    buflen = sizeof(buf);
    snprintf(filename, sizeof(filename), "/etc/%s_%s.conf",
             ACFG_WPS_CONFIG_PREFIX, ifname);
    fp = fopen(filename, "r");
    if (fp == NULL) {
        return -1;
    }

    while(fgets(buf, buflen, fp)) {
        pos = buf;
        if (strncmp(pos, "wps_state=", 10) == 0) {
            pos = strchr(buf, '=');
            pos++;
            val = atoi(pos);
            if (val == 2) {
                wps_cred->wps_state = val;
                ret = 1;
            }
        } else if (strncmp(pos, "ssid=", 5) == 0) {
            pos = strchr(buf, '=');
            pos++;
            memset(wps_cred->ssid, '\0', sizeof(wps_cred->ssid));
            snprintf(wps_cred->ssid, sizeof(wps_cred->ssid), "%s", pos);
        } else if (strncmp(pos, "wpa_key_mgmt=", 13) == 0) {
            pos = strchr(buf, '=');
            pos++;
            acfg_parse_wpa_key_mgmt(pos, wps_cred);
        } else if (strncmp(pos, "wpa_pairwise=", 13) == 0) {
            pos = strchr(buf, '=');
            pos++;
            acfg_parse_cipher(pos, wps_cred);
        } else if (strncmp(pos, "wpa_passphrase=", 15) == 0) {
            pos = strchr(buf, '=');
            pos++;
            len = strlen(pos);
            if (pos[len - 1] == '\n') {
                pos[len - 1] = '\0';
                len--;
            }
            memset(wps_cred->key, '\0', sizeof(wps_cred->key));
            acfg_os_strcpy((uint8_t *)wps_cred->key, (uint8_t *)pos, sizeof(wps_cred->key));
        } else if (strncmp(pos, "wpa_psk=", 7) == 0) {
            pos = strchr(buf, '=');
            pos++;
            len = strlen(pos);
            if (pos[len - 1] == '\n') {
                pos[len - 1] = '\0';
                len--;
            }
            memset(wps_cred->key, '\0', sizeof(wps_cred->key));
            acfg_os_strcpy((uint8_t *)wps_cred->key, (uint8_t *)pos, sizeof(wps_cred->key));
        } else if (strncmp(pos, "wpa=", 4) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->wpa = atoi(pos);
        } else if (strncmp(pos, "key_mgmt=", 9) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->key_mgmt = atoi(pos);
        } else if (strncmp(pos, "auth_alg=", 9) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->auth_alg = atoi(pos);
        } else if (strncmp(pos, "proto=", 6) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->wpa = atoi(pos);
        } else if (strncmp(pos, "wep_key=", 8) == 0) {
            pos = strchr(buf, '=');
            pos++;
            len = strlen(pos);
            if (pos[len - 1] == '\n') {
                pos[len - 1] = '\0';
                len--;
            }
            memset(wps_cred->wep_key, '\0', sizeof(wps_cred->wep_key));
            acfg_os_strcpy((uint8_t *)wps_cred->wep_key, (uint8_t *)pos, sizeof(wps_cred->wep_key));
        } else if (strncmp(pos, "wep_default_key=", 16) == 0) {
            pos = strchr(buf, '=');
            pos++;
            wps_cred->wep_key_idx = atoi(pos);
        }
    }
    fclose(fp);
    return ret;
}

void acfg_set_hs_iw_vap_param(acfg_wlan_profile_vap_params_t *vap_params)
{
    acfg_opmode_t opmode;
    acfg_macaddr_t mac_addr;

    memset(&mac_addr.addr, 0, sizeof(mac_addr.addr));
    acfg_get_opmode(vap_params->vap_name, &opmode);
    acfg_get_ap(vap_params->vap_name,&mac_addr);
    if(opmode == ACFG_OPMODE_HOSTAP)
    {
        if(vap_params->security_params.hs_iw_param.hessid[0] == 0)
	    snprintf(vap_params->security_params.hs_iw_param.hessid, ACFG_MACSTR_LEN, "%02X:%02X:%02X:%02X:%02X:%02X",mac_addr.addr[0], mac_addr.addr[1], mac_addr.addr[2], mac_addr.addr[3], mac_addr.addr[4], mac_addr.addr[5]);
        if(vap_params->security_params.hs_iw_param.network_type > 15)
            vap_params->security_params.hs_iw_param.network_type = DEFAULT_NETWORK_TYPE;
        if(vap_params->security_params.hs_iw_param.internet > 1)
            vap_params->security_params.hs_iw_param.internet = DEFAULT_INTERNET;
        if(vap_params->security_params.hs_iw_param.asra > 1)
            vap_params->security_params.hs_iw_param.asra = DEFAULT_ASRA;
        if(vap_params->security_params.hs_iw_param.esr > 1)
            vap_params->security_params.hs_iw_param.esr = DEFAULT_ESR;
        if(vap_params->security_params.hs_iw_param.uesa > 1)
            vap_params->security_params.hs_iw_param.uesa = DEFAULT_UESA;
        if(vap_params->security_params.hs_iw_param.venue_group >= VENUE_GROUP_RESERVED_START)
            vap_params->security_params.hs_iw_param.venue_group = DEFAULT_VENUE_GROUP;
        if(vap_params->security_params.hs_iw_param.roaming_consortium[0] == 0)
            vap_params->security_params.hs_iw_param.roaming_consortium[0] = '\0';
        if(vap_params->security_params.hs_iw_param.roaming_consortium2[0] == 0)
            vap_params->security_params.hs_iw_param.roaming_consortium2[0] = '\0';
        if(vap_params->security_params.hs_iw_param.venue_name[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.hs_iw_param.venue_name,
                           (uint8_t *)"venue_name=eng:Wi-Fi Alliance Labs\x0a 2989 Copper Road\x0aSanta Clara, CA 95051, USA",
                           sizeof(vap_params->security_params.hs_iw_param.venue_name));
    }
}

#define OFFSET(a,b) ((long )&((a *) 0)->b)

struct acfg_wps_params {
    uint8_t name[32];
    uint32_t offset;
    uint32_t size;
};

struct acfg_wps_params wps_device_info[] =
{
    {"wps_device_name",  OFFSET(acfg_wlan_profile_security_params_t, wps_device_name), ACFG_WSUPP_PARAM_LEN},
    {"wps_device_type",  OFFSET(acfg_wlan_profile_security_params_t, wps_device_type), ACFG_WSUPP_PARAM_LEN},
    {"wps_model_name",  OFFSET(acfg_wlan_profile_security_params_t, wps_model_name), ACFG_WSUPP_PARAM_LEN},
    {"wps_model_number",  OFFSET(acfg_wlan_profile_security_params_t, wps_model_number), ACFG_WSUPP_PARAM_LEN},
    {"wps_serial_number",  OFFSET(acfg_wlan_profile_security_params_t, wps_serial_number), ACFG_WSUPP_PARAM_LEN},
    {"wps_manufacturer",  OFFSET(acfg_wlan_profile_security_params_t, wps_manufacturer), ACFG_WSUPP_PARAM_LEN},
    {"wps_config_methods",  OFFSET(acfg_wlan_profile_security_params_t, wps_config_methods), ACFG_WPS_CONFIG_METHODS_LEN},
};

void acfg_set_wps_default_config(acfg_wlan_profile_vap_params_t *vap_params)
{
    acfg_opmode_t opmode;

    acfg_get_opmode(vap_params->vap_name, &opmode);
    if(opmode == ACFG_OPMODE_STA)
    {
        if(vap_params->security_params.wps_config_methods[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_config_methods,
                           (uint8_t *)"\"ethernet label push_button\"",
                           sizeof(vap_params->security_params.wps_config_methods));
        if(vap_params->security_params.wps_device_type[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_device_type, (uint8_t *)"1-0050F204-1",
                           sizeof(vap_params->security_params.wps_device_type));
        if(vap_params->security_params.wps_manufacturer[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_manufacturer, (uint8_t *)"Atheros",
                           sizeof(vap_params->security_params.wps_manufacturer));
        if(vap_params->security_params.wps_model_name[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_model_name, (uint8_t *)"cmodel",
                           sizeof(vap_params->security_params.wps_model_name));
        if(vap_params->security_params.wps_model_number[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_model_number, (uint8_t *)"123",
                           sizeof(vap_params->security_params.wps_model_number));
        if(vap_params->security_params.wps_serial_number[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_serial_number, (uint8_t *)"12345",
                           sizeof(vap_params->security_params.wps_serial_number));
        if(vap_params->security_params.wps_device_name[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_device_name, (uint8_t *)"WirelessClient",
                           sizeof(vap_params->security_params.wps_device_name));
    }
    else
    {
        if(vap_params->security_params.wps_config_methods[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_config_methods,
                           (uint8_t *)"push_button label virtual_display virtual_push_button physical_push_button",
                           sizeof(vap_params->security_params.wps_config_methods));
        if(vap_params->security_params.wps_device_type[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_device_type, (uint8_t *)"6-0050F204-1",
                           sizeof(vap_params->security_params.wps_device_type));
        if(vap_params->security_params.wps_manufacturer[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_manufacturer, (uint8_t *)"Atheros Communications, Inc.",
                           sizeof(vap_params->security_params.wps_manufacturer));
        if(vap_params->security_params.wps_model_name[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_model_name, (uint8_t *)"APxx",
                           sizeof(vap_params->security_params.wps_model_name));
        if(vap_params->security_params.wps_model_number[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_model_number, (uint8_t *)"APxx-xxx",
                           sizeof(vap_params->security_params.wps_model_number));
        if(vap_params->security_params.wps_serial_number[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_serial_number, (uint8_t *)"87654321",
                           sizeof(vap_params->security_params.wps_serial_number));
        if(vap_params->security_params.wps_device_name[0] == 0)
            acfg_os_strcpy((uint8_t *)vap_params->security_params.wps_device_name, (uint8_t *)"AtherosAP",
                           sizeof(vap_params->security_params.wps_device_name));
    }
}

void acfg_get_wps_dev_config(acfg_wlan_profile_vap_params_t *vap_params)
{
    FILE *fp;
    unsigned int i, offset;
    char buf[255], *pos;
    char filename[60];

    snprintf(filename,sizeof(filename),"/etc/%s_%s.conf", ACFG_WPS_DEV_CONFIG_PREFIX, vap_params->vap_name);

    fp = fopen(filename, "r");
    if(fp == NULL)
        return;

    while(fgets(buf, sizeof(buf), fp))
    {
        if(buf[0] == '#') {
            continue;
        }
        pos = buf;
        while (*pos != '\0') {
            if (*pos == '\n') {
                *pos = '\0';
                break;
            }
            pos++;
        }
        pos = strchr(buf, '=');
        if (pos == NULL) {
            continue;
        }
        *pos = '\0';
        pos++;
        for (i = 0; i < (sizeof (wps_device_info) /
                    sizeof (struct acfg_wps_params)); i++) {
            if (strcmp(buf, (char *)wps_device_info[i].name) == 0) {
                offset = wps_device_info[i].offset;
                acfg_os_strcpy((uint8_t *)(&vap_params->security_params) + offset, (uint8_t *)pos, wps_device_info[i].size);
                break;
            }
        }
    }
    fclose(fp);
}

void
acfg_update_wps_dev_config_file(acfg_wlan_profile_vap_params_t *vap_params, int force_update)
{
    char filename[60];
    FILE *fp;
    acfg_wlan_profile_security_params_t security_params;
    unsigned int i;

    snprintf(filename,sizeof(filename),"/etc/%s_%s.conf", ACFG_WPS_DEV_CONFIG_PREFIX, vap_params->vap_name);

    /* Try to open the file for reading */
    fp = fopen(filename, "r");
    if(fp == NULL)
    {
        /* Create file if it doesn't exist*/
        force_update = 1;
    }
    else
    {
        char buf[255];
        char *pos;
        int offset;

        /* make a copy of initial security_params, so that it can be used for later comparision */
        memcpy(&security_params, &vap_params->security_params, sizeof(acfg_wlan_profile_security_params_t));
        /* Read the contents and get the WPS device info */
        while(fgets(buf, sizeof(buf), fp))
        {
            if(buf[0] == '#') {
                continue;
            }
            pos = buf;
            while (*pos != '\0') {
                if (*pos == '\n') {
                    *pos = '\0';
                    break;
                }
                pos++;
            }
            pos =  strchr(buf, '=');
            if (pos == NULL) {
                continue;
            }
            *pos = '\0';
            pos++;
            for (i = 0; i < (sizeof (wps_device_info) /
                        sizeof (struct acfg_wps_params)); i++) {
                if (strcmp(buf, (char *)wps_device_info[i].name) == 0) {
                    offset = wps_device_info[i].offset;
                    acfg_os_strcpy((uint8_t *)(&security_params) + offset, (uint8_t *)pos, wps_device_info[i].size);
                    break;
                }
            }
        }
        if(memcmp(&security_params, &vap_params->security_params, sizeof(acfg_wlan_profile_security_params_t)))
        {
            /* Profile is updated, so update the WPS dev file too */
            force_update = 1;
        }
    }
    if(force_update == 1)
    {
        int ret, buflen;
        char str[255], data[ACFG_MAX_WPS_FILE_SIZE];
        int len = 0;

        if(fp){
            fclose(fp);
	    fp = NULL;
	}
	memset(data, '\0',ACFG_MAX_WPS_FILE_SIZE);
        buflen = sizeof(data);

        for(i = 0; i < (sizeof (wps_device_info) /
                    sizeof (struct acfg_wps_params)); i++)
        {
            ret = snprintf(str,sizeof(str), "\n%s=%s", wps_device_info[i].name,
                    (((uint8_t *)&(vap_params->security_params)) + wps_device_info[i].offset));
            if (ret >= 0 && buflen > ret)
            {
                strlcat(data, str, sizeof(data));
                buflen -= ret;
                len += ret;
            }
        }
        acfg_update_wps_config_file(vap_params->vap_name, ACFG_WPS_DEV_CONFIG_PREFIX, data, len);
    }
    if(fp)
	fclose(fp);
}

void
acfg_update_wps_config_file(uint8_t *ifname, char *prefix, char *data, int len)
{
    char filename[32];
    FILE *fp;
    char *pos, *start;
    int ret = 0;

    snprintf(filename, sizeof(filename), "/etc/%s_%s.conf", prefix, ifname);
    fp = fopen(filename, "w");
    if (fp == NULL){
	return;
    }
    pos = start = data;
    while (len) {
        start = pos;
        while ((*pos != '\n') && *pos != '\0') {
            pos++;
            len--;
        }
        if (*pos == '\0') {
            ret = 1;
        }
        *pos = '\0';
        fprintf(fp, "%s\n", start);
        if (ret == 1) {
            fclose(fp);
            return;
        }
        pos++;
        len--;
        while(*pos == '\n') {
            pos++;
            len--;
        }
    }
    fclose(fp);
}

uint32_t
acfg_wps_success_cb(uint8_t *ifname)
{
    char data[ACFG_MAX_WPS_FILE_SIZE];
    int datalen =  0;
    acfg_wps_cred_t wps_cred;
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_opmode_t opmode;
    acfg_wlan_profile_vap_params_t vap_params;

    status = acfg_get_opmode(ifname, &opmode);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Opmode fetch fail for %s\n", __func__, ifname);
        return QDF_STATUS_E_FAILURE;
    }
    memset(&vap_params, 0, sizeof(acfg_wlan_profile_vap_params_t));
    memset(data, '\0', sizeof(data));
    status = acfg_get_wps_cred(ifname, opmode, data, &datalen);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Get WPS credentials failed for %s\n", __func__, ifname);
        return QDF_STATUS_E_FAILURE;
    }
    acfg_update_wps_config_file(ifname, ACFG_WPS_CONFIG_PREFIX, data, datalen);
    acfg_os_strcpy((uint8_t *)vap_params.vap_name, (uint8_t *)ifname,
                   sizeof(vap_params.vap_name));
    if (opmode == ACFG_OPMODE_STA) {
        return QDF_STATUS_SUCCESS;
    }
    acfg_get_wps_config(ifname, &wps_cred);
    acfg_get_wps_dev_config(&vap_params);
    if (wps_cred.wps_state == WPS_FLAG_CONFIGURED) {
        acfg_os_strcpy((uint8_t *)vap_params.vap_name,(uint8_t *)ifname,
                       sizeof(vap_params.vap_name));
        status = acfg_set_wps_vap_params(&vap_params, &wps_cred);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
        status = acfg_config_security(&vap_params);
        if (status != QDF_STATUS_SUCCESS) {
            return QDF_STATUS_E_FAILURE;
        }
    }
    return QDF_STATUS_SUCCESS;
}

uint32_t acfg_get_iface_list(acfg_vap_list_t *list, int *count)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t	req = {.cmd = ACFG_REQ_GET_PROFILE};
    acfg_radio_vap_info_t *ptr;
    uint8_t wifi_iface[4][ACFG_MAX_IFNAME] = {"wifi0", "wifi1", "wifi2", "wifi3"};
    unsigned int n;
    int num_iface = 0, i;

    for (n = 0; n < sizeof (wifi_iface) / sizeof(wifi_iface[0]); n++) {
        status = acfg_wlan_iface_present((char *)wifi_iface[n]);
        if(status != QDF_STATUS_SUCCESS) {
            continue;
        }
        ptr = (acfg_radio_vap_info_t *)req.data;
        memset(ptr, 0 , sizeof(acfg_radio_vap_info_t));
        if (acfg_os_check_str(wifi_iface[n], ACFG_MAX_IFNAME))
            return QDF_STATUS_E_NOENT;
        status = acfg_os_send_req(wifi_iface[n], &req);

        if (status == QDF_STATUS_SUCCESS) {
            for (i = 0; i <  ptr->num_vaps; i++) {

                acfg_os_strcpy((uint8_t *)list->iface[i + num_iface], (uint8_t *)ptr->vap_info[i].vap_name, ACFG_MAX_IFNAME);
            }
            num_iface += i;
        }
    }
    *count = num_iface;
    return QDF_STATUS_SUCCESS;
}

uint32_t
acfg_handle_wps_event(uint8_t *ifname, enum acfg_event_handler_type event)
{
    uint32_t status = QDF_STATUS_SUCCESS;
    acfg_opmode_t opmode;

    status = acfg_get_opmode(ifname, &opmode);
    if (status != QDF_STATUS_SUCCESS) {
        return status;
    }
    switch (event) {
        case ACFG_EVENT_WPS_NEW_AP_SETTINGS:
            if (opmode == ACFG_OPMODE_HOSTAP) {
                status = acfg_wps_success_cb(ifname);
            }
            break;
        case ACFG_EVENT_WPS_SUCCESS:
            if (opmode == ACFG_OPMODE_STA) {
                status = acfg_wps_success_cb(ifname);
            }
            break;
        default:
            return QDF_STATUS_E_NOSUPPORT;
    }
    return status;
}

uint32_t
acfg_set_wps_pin(char *ifname, int action, char *pin, char *pin_txt,
        char *bssid)
{
    char cmd[255];
    char replybuf[255];
    uint32_t len = 0;
    acfg_opmode_t opmode;
    acfg_vap_list_t vap_list;

    memset(replybuf, 0, sizeof(replybuf));
    if (acfg_get_opmode((uint8_t *)ifname, &opmode) != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("Opmode get failed\n");
        return -1;
    }
    acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
            ctrl_wpasupp);
    acfg_os_strcpy((uint8_t *)vap_list.iface[0], (uint8_t *)ifname, ACFG_MAX_IFNAME);
    vap_list.num_iface = 1;
    if (action == ACFG_WPS_PIN_SET) {
        snprintf(cmd, sizeof(cmd), "%s %s", WPA_WPS_CHECK_PIN_CMD_PREFIX, pin);
        len = sizeof (replybuf);
        if(acfg_ctrl_req((uint8_t *)ifname, cmd, strlen(cmd), replybuf,
                    &len, opmode) < 0){
            acfg_log_errstr("%s: Failed to set WPS pin for %s\n", __func__, ifname);
            return QDF_STATUS_E_FAILURE;
        }
        if (!strncmp(replybuf, "FAIL-CHECKSUM", strlen("FAIL-CHECKSUM")) ||
                !strncmp(replybuf, "FAIL", strlen("FAIL")))
        {
            acfg_log_errstr("%s: Invalid pin\n", __func__);
            return QDF_STATUS_E_FAILURE;
        }
    }
    if (opmode == ACFG_OPMODE_HOSTAP) {
        if (action == ACFG_WPS_PIN_SET) {
            memset(replybuf, '\0', sizeof (replybuf));
            len = sizeof (replybuf);
            snprintf(cmd, sizeof(cmd), "%s %s %s %d", WPA_WPS_PIN_CMD_PREFIX,  "any",
                     pin, WPS_TIMEOUT);
            if(acfg_ctrl_req((uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_HOSTAP) < 0){
                return QDF_STATUS_E_FAILURE;
            }
            acfg_os_strcpy((uint8_t *)pin_txt, (uint8_t *)replybuf, 10);

            memset(replybuf, '\0', sizeof (replybuf));
            len = sizeof (replybuf);
            snprintf(cmd, sizeof(cmd), "%s %s %s %d",
                     WPA_WPS_AP_PIN_CMD_PREFIX,  "set",
                     pin, WPS_TIMEOUT);
            if(acfg_ctrl_req((uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_HOSTAP) < 0){
                return QDF_STATUS_E_FAILURE;
            }
            acfg_os_strcpy((uint8_t *)pin_txt, (uint8_t *)replybuf, 10);
        } else if (action == ACFG_WPS_PIN_RANDOM) {
            memset(replybuf, '\0', sizeof (replybuf));
            len = sizeof (replybuf);
            snprintf(cmd, sizeof(cmd), "%s %s %d",
                     WPA_WPS_AP_PIN_CMD_PREFIX,  "random",
                     WPS_TIMEOUT);
            if(acfg_ctrl_req((uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_HOSTAP) < 0){
                return QDF_STATUS_E_FAILURE;
            }
            acfg_log_errstr("PIN: %s\n", replybuf);
            acfg_os_strcpy((uint8_t *)pin_txt, (uint8_t *)replybuf, 10);
        }
    } else if (opmode == ACFG_OPMODE_STA) {
        char bssid_str[20];
        uint8_t macaddr[6];

        if (action == ACFG_WPS_PIN_SET) {
            if (hwaddr_aton(bssid, macaddr) == -1) {
                snprintf(bssid_str, sizeof(bssid_str), "any");
            } else {
                snprintf(bssid_str, sizeof(bssid_str), "%s", bssid);
            }
            snprintf(cmd, sizeof(cmd), "%s %s %s", WPA_WPS_PIN_CMD_PREFIX,
                    bssid_str, pin);
            if(acfg_ctrl_req((uint8_t *)ifname, cmd, strlen(cmd), replybuf, &len,
                        ACFG_OPMODE_STA) < 0){
                return QDF_STATUS_E_FAILURE;
            }
        }
    }
    return QDF_STATUS_SUCCESS;
}

uint32_t
acfg_set_wps_pbc(char *ifname)
{
    acfg_opmode_t opmode;
    char cmd[255], replybuf[255];
    uint32_t len = 0;
    uint32_t status = QDF_STATUS_SUCCESS;

    memset(replybuf, 0, sizeof(replybuf));
    len = sizeof(replybuf);
    memset(cmd, '\0', sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "%s", WPA_WPS_PBC_CMD_PREFIX);

    status = acfg_get_opmode((uint8_t *)ifname, &opmode);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Opmode fetch fail\n", ifname);
        return QDF_STATUS_E_FAILURE;
    }

    acfg_get_ctrl_iface_path(ACFG_CONF_FILE, ctrl_hapd,
            ctrl_wpasupp);
    if(acfg_ctrl_req((uint8_t *)ifname, cmd, strlen(cmd),
                replybuf, &len,
                opmode) < 0){
        return QDF_STATUS_E_FAILURE;
    }
    if(strncmp(replybuf, "OK", 2) != 0) {
        acfg_log_errstr("set pbc failed for %s\n", ifname);
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

/**
 * @brief Set preamble
 *
 * @param vap_name (VAP interface)
 * @param preamble - long or short preamble  0-long preamble 1-short preamble
 *
 * @return
 */
uint32_t
acfg_set_preamble(uint8_t  *vap_name, uint32_t preamble)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    status = acfg_set_vap_param(vap_name, ACFG_PARAM_SHORTPREAMBLE, preamble);
    return status;
}

/**
 * @brief Set slot time
 *
 * @param vap_name (VAP interface)
 * @param shot - long or short slot time  0-long slot 1-short slot
 *
 * @return
 */
uint32_t
acfg_set_slot_time(uint8_t  *vap_name, uint32_t slot)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    status = acfg_set_vap_param(vap_name, ACFG_PARAM_SHORT_SLOT, slot);
    return status;
}

/**
 * @brief Set ERP
 *
 * @param vap_name (VAP interface)
 * @param erp - 0-disable ERP 1-enable ERP
 *
 * @return
 */
uint32_t
acfg_set_erp(uint8_t  *vap_name, uint32_t erp)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    status = acfg_set_vap_param(vap_name, ACFG_PARAM_ERP, erp);
    return status;
}

/**
 * @brief Set regdomain
 *
 * @param wifi_name (physical radio interface name)
 * @param regdomain
 *
 * @return
 */
uint32_t
acfg_set_regdomain(uint8_t  *wifi_name, uint32_t regdomain)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    status = acfg_set_radio_param(wifi_name, ACFG_PARAM_RADIO_REGDOMAIN, regdomain);
    return status;
}

/**
 * @brief Set ratemask of the VAP
 *
 * @param
 * @vap_name VAP interface
 * @preamble: 0 - legacy, 1 - HT, 2 - VHT
 * @mask_lower32: lower 32-bit mask
 * @mask_higher32: higher 32-bit mask
 * @mask_lower32: lower_2 32-bit mask
 *
 * @return
 */
uint32_t
acfg_set_ratemask(uint8_t  *vap_name, uint32_t preamble, uint32_t mask_lower32,
                  uint32_t mask_higher32, uint32_t mask_lower32_2)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_SET_RATEMASK};
    acfg_ratemask_t   *ptr;

    ptr = (acfg_ratemask_t *)req.data;
    ptr->preamble = (uint8_t)(preamble & 0xFF);
    ptr->mask_lower32 = mask_lower32;
    ptr->mask_higher32 = mask_higher32;
    ptr->mask_lower32_2 = mask_lower32_2;

    status = acfg_os_send_req(vap_name, &req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: set ratemask failed! \n", vap_name);
        return QDF_STATUS_E_FAILURE;
    }

    return status;
}

/**
 * @brief add Beacon app IE, add inidividual IEs into Beacon/Probe Response frames (AP)
 *
 * @param
 * @vap_name VAP interface
 * @ie: Information element
 * @ie_len: Length of the IE buffer in octets
 *
 * @return
 */
uint32_t
acfg_add_app_ie(uint8_t  *vap_name, const uint8_t *ie, uint32_t ie_len)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   *req = NULL;
    acfg_appie_t    *ptr;

    req = malloc(sizeof(acfg_os_req_t) + ie_len);
    if (req == NULL) {
        acfg_log_errstr("%s: mem alloc failure\n", __FUNCTION__);
        return status;
    }
    memset(req, 0, sizeof(acfg_os_req_t) + ie_len);
    req->cmd = ACFG_REQ_SET_APPIEBUF;
    ptr = (acfg_appie_t *)req->data;

    memcpy(ptr->buf, ie, ie_len);
    ptr->buflen = ie_len;

    ptr->frmtype = ACFG_FRAME_BEACON;
    status = acfg_os_send_req(vap_name, req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: add app ie(type: ACFG_FRAME_BEACON) failed! \n", vap_name);
        return QDF_STATUS_E_FAILURE;
    }

    ptr->frmtype = ACFG_FRAME_PROBE_RESP;
    status = acfg_os_send_req(vap_name, req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: add app ie(type: ACFG_FRAME_PROBE_RESP) failed! \n", vap_name);
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

uint32_t
acfg_set_chanswitch(uint8_t  *wifi_name, uint8_t  chan_num)
{
    uint32_t       status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_DOTH_CHSWITCH};
    acfg_chan_t        *ptr;

    ptr = (acfg_chan_t *)req.data;

    if (acfg_os_check_str(wifi_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    *ptr = chan_num;

    status = acfg_os_send_req(wifi_name, &req);

    return status;

}

/**
 * @brief tx99 wrapper, call the actual tx99 tool
 *
 * @param
 * @wifi_name physical radio interface
 * @tx99_data acfg_tx99_data_t structure
 *
 * @return
 */
uint32_t
acfg_tx99_tool(uint8_t  *wifi_name, acfg_tx99_data_t* tx99_data)
{
    uint32_t  status = QDF_STATUS_SUCCESS;
    char *argv[20];
    char interface_opt[3];
    char interface[6];
    char tx99_cmd[12];
    char tx99_mode_opt[6];
    char tx99_mode[10];
    char freq_opt[10];
    char freq[12];
    char chain_opt[10];
    char chain[12];
    char rate_opt[10];
    char rate[12];
    char mode_opt[8];
    char mode[16];
    char power_opt[8];
    char power[12];
    char pattern_opt[12];
    char pattern[12];
    char shortguard_opt[14] = "";

    snprintf(interface_opt, sizeof(interface_opt), "%s", "-i");
    snprintf(interface, sizeof(interface), "%s", wifi_name);
    snprintf(tx99_cmd, sizeof(tx99_cmd), "%s", "athtestcmd");
    argv[0]= tx99_cmd;
    argv[1]= interface_opt;
    argv[2]= interface;

    if(IS_TX99_TX(tx99_data->flags)){
        //tx99 TX
        if(!IS_TX99_TX_ENABLED(tx99_data->flags)){
            snprintf(tx99_mode_opt, sizeof(tx99_mode_opt), "%s", "--tx");
            snprintf(tx99_mode, sizeof(tx99_mode), "%s", "off");
            argv[3]= tx99_mode_opt;
            argv[4]= tx99_mode;
            argv[5]= (char*)0;
        }else{
            snprintf(tx99_mode_opt, sizeof(tx99_mode_opt), "%s", "--tx");
            snprintf(tx99_mode, sizeof(tx99_mode), "%s", "tx99");
            snprintf(freq_opt, sizeof(freq_opt), "%s", "--txfreq");
            snprintf(freq, sizeof(freq), "%d", tx99_data->freq);
            snprintf(chain_opt, sizeof(chain_opt), "%s", "--txchain");
            snprintf(chain, sizeof(chain), "%d", tx99_data->chain);
            snprintf(rate_opt, sizeof(rate_opt), "%s", "--txrate");
            snprintf(rate,sizeof(rate), "%d", tx99_data->rate);
            snprintf(mode_opt, sizeof(mode_opt), "%s", "--mode");
            snprintf(mode, sizeof(mode), "%s", tx99_data->mode);
            snprintf(power_opt, sizeof(power_opt), "%s", "--txpwr");
            snprintf(power, sizeof(power), "%d", tx99_data->power);
            snprintf(pattern_opt, sizeof(pattern_opt), "%s", "--txpattern");
            snprintf(pattern, sizeof(pattern), "%d", tx99_data->pattern);
            if(tx99_data->shortguard == 1 ){
                snprintf(shortguard_opt, sizeof(shortguard_opt),
                         "%s", "--shortguard");
            }
            argv[3]= tx99_mode_opt;
            argv[4]= tx99_mode;
            argv[5]= freq_opt;
            argv[6]= freq;
            argv[7]= chain_opt;
            argv[8]= chain;
            argv[9]= rate_opt;
            argv[10]= rate;
            argv[11]= mode_opt;
            argv[12]= mode;
            argv[13]= power_opt;
            argv[14]= power;
            argv[15]= pattern_opt;
            argv[16]= pattern;
            argv[17]= shortguard_opt;
            argv[18]= (char *)0;
        }
    }else{
        //tx99 RX
        if(IS_TX99_RX_REPORT(tx99_data->flags)){
            snprintf(tx99_mode_opt, sizeof(tx99_mode_opt), "%s", "--rx");
            snprintf(tx99_mode, sizeof(tx99_mode), "%s", "report");
            argv[3]= tx99_mode_opt;
            argv[4]= tx99_mode;
            argv[5]= (char*)0;
        }else{
            snprintf(tx99_mode_opt, sizeof(tx99_mode_opt), "%s", "--rx");
            snprintf(tx99_mode, sizeof(tx99_mode), "%s", "promis");
            snprintf(freq_opt, sizeof(freq_opt), "%s", "--rxfreq");
            snprintf(freq, sizeof(freq), "%d", tx99_data->freq);
            snprintf(chain_opt, sizeof(chain_opt), "%s", "--rxchain");
            snprintf(chain, sizeof(chain), "%d", tx99_data->chain);
            snprintf(mode_opt, sizeof(mode_opt), "%s", "--mode");
            snprintf(mode, sizeof(mode), "%s", tx99_data->mode);
            argv[3]= tx99_mode_opt;
            argv[4]= tx99_mode;
            argv[5]= freq_opt;
            argv[6]= freq;
            argv[7]= chain_opt;
            argv[8]= chain;
            argv[9]= mode_opt;
            argv[10]= mode;
            argv[11]= (char *)0;
        }
    }
#if QCA_DEBUG_TX99
    printf("###########dumping argv###############\n");
    int i;
    for(i=0; argv[i] != NULL;i++){
        printf("%s ",argv[i]);
    }
    printf("\n");
#endif
    if(fork()==0){
        status = execvp(tx99_cmd, argv);
    }

    return status;
}
/**
 * @brief Enable MU Grouping per tidmask
 *
 * @param vap_name  (Upto ACFG_MAX_IFNAME)
 *        mac
 *        tidmask
 *
 * @return On Success
 *             If : QDF_STATUS_SUCCESS
 *             Else: QDF_STATUS_E_FAILURE
 *         On Error: A_STATUS_EFAULT
 */
uint32_t
acfg_set_mu_whtlist(uint8_t *vap_name, uint8_t *mac, uint16_t tidmask)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t      req = {.cmd = ACFG_REQ_SET_MU_WHTLIST};
    acfg_set_mu_whtlist_t    *ptr;

    ptr     = (acfg_set_mu_whtlist_t *)req.data;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    memcpy(ptr->macaddr, mac, ACFG_MACADDR_LEN);
    ptr->tidmask = tidmask;

    status = acfg_os_send_req(vap_name, &req);

    return status;
}


/**
 * @brief Set mode based transmit power
 *
 * @param
 * @wifi_name physical radio interface name
 * @mode: 0 for 5G, 1 for 2G
 * @power: new transmit power which applies across the band
 *
 * @return
 */

#define POWER_LIMIT_UPPER   50.0
#define POWER_LIMIT_LOWER   -10.0


uint32_t
acfg_set_tx_power(uint8_t  *wifi_name, uint32_t mode, float power)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    uint32_t tx_power;
    tx_power = (int32_t) (2*power);
    acfg_log_errstr("%s[%d] mode = %d, power = %d\n", __func__,__LINE__, mode, tx_power);


    if((power>POWER_LIMIT_UPPER) || (power<POWER_LIMIT_LOWER))
    {
        acfg_log_errstr("Failed power limit test. Valid range (%f, %f)\n", POWER_LIMIT_UPPER, POWER_LIMIT_LOWER);
        return QDF_STATUS_E_FAILURE;
    }


    if(mode == 0)
    {
        status = acfg_set_radio_param(wifi_name, ACFG_PARAM_RADIO_TX_POWER_5G, tx_power);
    }
    else if (mode == 1)
    {
        status = acfg_set_radio_param(wifi_name, ACFG_PARAM_RADIO_TX_POWER_2G, tx_power);
    }
    else
    {
        acfg_log_errstr("%s[%d]:: Incorrect mode = %d\n", __func__,__LINE__,mode);
        status = QDF_STATUS_E_FAILURE;
    }

    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: set power failed! \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

#undef POWER_LIMIT_UPPER
#undef POWER_LIMIT_LOWER


/**
 * @brief Set sensitivity level in dBm
 *
 * @param
 * @wifi_name physical radio interface name
 * @sens_level: Sensitivity level in dBm
 *
 * @return
 */

#define SENS_LIMIT_UPPER    -10
#define SENS_LIMIT_LOWER    -95

uint32_t
acfg_set_sens_level(uint8_t  *wifi_name, float sens_level)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    uint32_t level=(int32_t)sens_level;
    acfg_log_errstr("%s[%d] sens_level = 0x%x\n", __func__, __LINE__, level);

    if((sens_level>SENS_LIMIT_UPPER) || (sens_level<SENS_LIMIT_LOWER))
    {
        acfg_log_errstr("Failed sens limit test. Valid range (%d, %d)\n", SENS_LIMIT_UPPER, SENS_LIMIT_LOWER);
        return QDF_STATUS_E_FAILURE;
    }

    status = acfg_set_radio_param(wifi_name, ACFG_PARAM_RADIO_SENS_LEVEL, level);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: set sens level failed! \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}
#undef SENS_LIMIT_UPPER
#undef SENS_LIMIT_LOWER

/**
 * @brief CCA Thresh hold set command
 *
 * @param
 * @wifi_name physical radio interface name
 * @cca_threshold: CCA power in dBm
 *
 * @return
 */

#define CCA_THRESHOLD_LIMIT_UPPER  -10
#define CCA_THRESHOLD_LIMIT_LOWER  -95

uint32_t
acfg_set_cca_threshold(uint8_t  *wifi_name, float cca_threshold)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    uint32_t  threshold =(int32_t)cca_threshold;
    acfg_log_errstr("%s[%d] CCA threshold = 0x%x\n", __func__, __LINE__, threshold);

    if((cca_threshold>CCA_THRESHOLD_LIMIT_UPPER) || (cca_threshold<CCA_THRESHOLD_LIMIT_LOWER))
    {
        acfg_log_errstr("Failed cca threshold limit test. Valid range (%d, %d)\n", CCA_THRESHOLD_LIMIT_UPPER, CCA_THRESHOLD_LIMIT_LOWER);
        return QDF_STATUS_E_FAILURE;
    }

    status = acfg_set_radio_param(wifi_name, ACFG_PARAM_RADIO_CCA_THRESHOLD, threshold);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: set cca threshold failed! \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}
#undef CCA_THRESHOLD_LIMIT_UPPER
#undef CCA_THRESHOLD_LIMIT_LOWER


/**
 * @brief get rssi to dbm conversion factor
 *
 * @param
 * @wifi_name physical radio interface name
 *
 * @return
 */

uint32_t
acfg_get_nf_dbr_dbm_info(uint8_t  *wifi_name)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_NF_DBR_DBM_INFO};
    status = acfg_os_send_req(wifi_name, &req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: NF dbr dbm info request failed \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

/**
 * @brief get per packet power dBm
 *
 * @param
 * @wifi_name physical radio interface name
 *
 * @return
 */

uint32_t
acfg_get_packet_power_info(uint8_t  *wifi_name, acfg_packet_power_param_t *param)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GET_PACKET_POWER_INFO};
    acfg_packet_power_param_t *ptr;

    ptr = (acfg_packet_power_param_t *)req.data;
    ptr->chainmask = param->chainmask;
    ptr->chan_width = param->chan_width;
    ptr->rate_flags = param->rate_flags;
    ptr->su_mu_ofdma = param->su_mu_ofdma;
    ptr->nss = param->nss;
    ptr->preamble = param->preamble;
    ptr->hw_rate = param->hw_rate;

    status = acfg_os_send_req(wifi_name, &req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: Per packet power info request failed \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }
    return status;
}

/**
 * @brief config GPIO command
 *
 * @param
 * @wifi_name physical radio interface name
 * @gpio_num: GPIO pin number
 * @input: 1 for input, 0 for output
 * @pull_type: Pull type
 * @intr_mode: Interrupt mode
 *
 * @return
 */

uint32_t
acfg_gpio_config(uint8_t  *wifi_name, uint32_t gpio_num, uint32_t input, uint32_t pull_type, uint32_t intr_mode)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GPIO_CONFIG};
    acfg_gpio_config_t *ptr;

    ptr = (acfg_gpio_config_t *)req.data;
    ptr->gpio_num = gpio_num;
    ptr->input = input;
    ptr->pull_type = pull_type;
    ptr->intr_mode = intr_mode;

    status = acfg_os_send_req(wifi_name, &req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: GPIO config failed! \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }

    return status;
}

/**
 * @brief GPIO output set command
 *
 * @param
 * @wifi_name physical radio interface name
 * @gpio_num: GPIO pin number
 * @set: 1 for set, 0 for unset
 *
 * @return
 */

uint32_t
acfg_gpio_set(uint8_t  *wifi_name, uint32_t gpio_num, uint32_t set)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t       req = {.cmd = ACFG_REQ_GPIO_SET};
    acfg_gpio_set_t     *ptr;

    ptr = (acfg_gpio_set_t *)req.data;
    ptr->gpio_num = gpio_num;
    ptr->set = set;

    status = acfg_os_send_req(wifi_name, &req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: GPIO set failed! \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }

    return status;
}



/**
 * @brief CLT table set command
 *
 * @param
 * @wifi_name physical radio interface name
 * @mode: 0 for 5G and 1 for 2G
 * @len: length of the table
 * @ctl_table: pointer to CTL table
 *
 * @return
 */

uint32_t
acfg_set_ctl_table(uint8_t  *wifi_name, uint32_t band, uint32_t len, uint8_t  *ctl_table)
{
    uint32_t  status = QDF_STATUS_E_FAILURE;
    acfg_ctl_table_t     *ptr;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SET_CTL_TABLE};

    ptr = (acfg_ctl_table_t *)req.data;
    ptr->band = band;
    ptr->len  = len;

    if(band != 0 && band != 1)
    {
        acfg_log_errstr("Invalid band. It can only be 0 or 1\n");
        return QDF_STATUS_E_FAILURE;
    }

    memcpy(&ptr->ctl_tbl[0], ctl_table, len);
    status = acfg_os_send_req(wifi_name, &req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: CTL Table set failed! \n", wifi_name);
        return QDF_STATUS_E_FAILURE;
    }

    return status;
}

static char buffer[MAX_PAYLOAD];

int acfg_ifname_index(uint8_t *name) {
    uint8_t *cp;
    int unit;

    for (cp = name; *cp != '\0' && !('0' <= *cp && *cp <= '9'); cp++)
        ;
    if (*cp != '\0')
    {
        unit = 0;
        for (; *cp != '\0'; cp++)
        {
            if (!('0' <= *cp && *cp <= '9'))
                return -1;
            unit = (unit * 10) + (*cp - '0');
        }
    }
    else
        unit = -1;
    return unit;
}

uint32_t
acfg_send_raw_pkt(uint8_t  *vap_name, uint8_t *pkt_buf, uint32_t len, uint8_t type, uint16_t chan, uint8_t nss, uint8_t preamble, uint8_t mcs, uint8_t retry, uint8_t power, uint16_t scan_dur)
{
    uint32_t  status = QDF_STATUS_SUCCESS;
    int acfg_driver_sock;
    int on = 16*1024 ;
    int return_status;
    struct sockaddr_nl src_addr;
    struct acfg_offchan_hdr *acfg_hdr;
    uint32_t written, recvlen;
    struct acfg_offchan_tx_hdr *acfg_frame;

    if (len > (MAX_PAYLOAD - sizeof(struct acfg_offchan_hdr) - sizeof(struct nlmsghdr)
                          - sizeof(struct acfg_offchan_tx_hdr))) {
        acfg_log_errstr("%s: Bad length\n", __FUNCTION__);
        return -1;
    }
    if (strlen((char*)vap_name) >= ACFG_MAX_IFNAME)
        return -1;

    /* create a netlink socket */
    acfg_driver_sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ACFG);
    if (acfg_driver_sock < 0) {
        acfg_log_errstr("socket errno=%d\n", acfg_driver_sock);
        return acfg_driver_sock;
    }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    src_addr.nl_groups = 0;

    return_status = setsockopt(acfg_driver_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(return_status < 0) {
        acfg_log_errstr("nl socket option failed\n");
        close(acfg_driver_sock);
        return return_status;
    }
    return_status = bind(acfg_driver_sock, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(return_status < 0) {
        acfg_log_errstr("BIND errno=%d\n", return_status);
        close(acfg_driver_sock);
        return return_status;
    }

    nlh = (struct nlmsghdr *)buffer;
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(*acfg_hdr) + sizeof(*acfg_frame) + len);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    acfg_hdr = NLMSG_DATA(nlh);
    acfg_os_strcpy((uint8_t *)acfg_hdr->vap_name, (uint8_t *)vap_name, ACFG_MAX_IFNAME);

    acfg_hdr->msg_type = type;
    acfg_hdr->msg_length = len;
    acfg_hdr->channel = chan;
    acfg_hdr->scan_dur = scan_dur;
    acfg_hdr->num_frames = 1;

    acfg_log_errstr("\n sending the info to driver with sock no. %d\n",acfg_driver_sock);
    acfg_frame = (struct acfg_offchan_tx_hdr *) (acfg_hdr +1);
    acfg_frame->id = 1;
    acfg_frame->type = type;
    acfg_frame->length = len;
    acfg_frame->nss = nss;
    acfg_frame->preamble = preamble;
    acfg_frame->mcs = mcs;
    acfg_frame->retry = retry;
    acfg_frame->power = power;

    memcpy((acfg_frame + 1), pkt_buf, len);

    /* send the msg buffer to the driver */
    written = write(acfg_driver_sock, ((char *)buffer),
                    sizeof(struct nlmsghdr) + sizeof(struct acfg_offchan_hdr) +
                    sizeof(struct acfg_offchan_tx_hdr) + len);
    if ( written < len + sizeof(struct acfg_offchan_hdr) + sizeof(struct acfg_offchan_tx_hdr)) {
        // TBD : Need to write pending data if there is partial write
        acfg_log_errstr("Partial write. Closing connection. Size: %d Written: %d\n", len + sizeof(struct acfg_offchan_hdr), written);
        close(acfg_driver_sock);
        acfg_driver_sock = -1;
    }

 again:
    /* wait for a response from the driver */
	recvlen = recv(acfg_driver_sock, buffer, MAX_PAYLOAD, 0);
	if( recvlen <= 0 ) {
        acfg_log_errstr("Nothing to receive\n");
	} else {
        acfg_hdr = (struct acfg_offchan_hdr *) ((char *)buffer + sizeof(struct nlmsghdr));
        acfg_offchan_stat_t *offchan_stat;
        offchan_stat = (acfg_offchan_stat_t *) ((char *)buffer + sizeof(struct nlmsghdr) + sizeof(struct acfg_offchan_hdr));
        if (acfg_hdr->msg_type == ACFG_CHAN_FOREIGN) {
            acfg_log_errstr("Foreign channel\n");
            goto again;
        } else if (acfg_hdr->msg_type == ACFG_CHAN_HOME) {
            acfg_log_errstr("Home channel\n");
            goto again;
        } else {
            acfg_log_errstr("Tx status: %d\n", acfg_hdr->msg_type);
            printf("Dwell time %dus Home to Foreign switch time %dus Foreign to Home switch time %dus\n",
                   offchan_stat->dwell_time, offchan_stat->chanswitch_time_htof, offchan_stat->chanswitch_time_ftoh);
        }
    }
    return status;
}


#if QCA_SUPPORT_GPR
/**
 * @brief start_gpr on a radio and enable for given vap
 *
 * @param
 * @vap_name VAP interface
 * @ pkt_buf pointer to buffer send from applications
 * @ len length of buffer sent
 * @ period periodicity of GPR packets in msec
 * @ nss nss of GPR packet
 * preamble preamble of GPR packet
 * mcs mcs of GPR packet
 *
 * @return
 */

uint32_t
acfg_start_gpr(uint8_t  *vap_name, uint8_t *pkt_buf, uint32_t len, uint32_t period, uint8_t nss, uint8_t preamble, uint8_t mcs)
{
    struct sockaddr_nl src_addr;
    struct acfg_offchan_hdr *acfg_hdr;
    struct acfg_offchan_tx_hdr *acfg_frame;
    uint32_t  status = QDF_STATUS_SUCCESS;
    uint32_t written;
    int acfg_driver_sock;
    int on = 16*1024 ;
    int return_status;
    int total_len;

    total_len = len + sizeof(struct acfg_offchan_tx_hdr) + sizeof(struct acfg_offchan_hdr)
                                    + sizeof(struct nlmsghdr);

    if (total_len > MAX_PAYLOAD) {
        acfg_log_errstr("%s: Bad length of %d, Max payload size is %d bytes \n", __FUNCTION__, len, MAX_PAYLOAD);
        return -1;
    }
    if (strlen((char*)vap_name) >= ACFG_MAX_IFNAME)
        return -1;

    /* create a netlink socket */
    acfg_driver_sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ACFG);
    if (acfg_driver_sock < 0) {
        acfg_log_errstr("socket errno=%d\n", errno);
        return acfg_driver_sock;
    }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    src_addr.nl_groups = 0;

    return_status = setsockopt(acfg_driver_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(return_status < 0) {
        acfg_log_errstr("nl socket option failed\n");
        close(acfg_driver_sock);
        return return_status;
    }
    return_status = bind(acfg_driver_sock, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(return_status < 0) {
        acfg_log_errstr("BIND errno=%d\n", errno);
        close(acfg_driver_sock);
        return return_status;
    }

    nlh = (struct nlmsghdr *)buffer;
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(*acfg_hdr) + sizeof(*acfg_frame) + len);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    acfg_hdr = NLMSG_DATA(nlh);
    acfg_os_strcpy((uint8_t *)acfg_hdr->vap_name, (uint8_t *)vap_name, ACFG_MAX_IFNAME);

    acfg_hdr->msg_type = ACFG_PKT_TYPE_GPR;
    acfg_hdr->msg_length = len;

    acfg_log_errstr("\n sending the info to driver with sock no. %d\n",acfg_driver_sock);
    acfg_frame = (struct acfg_offchan_tx_hdr *) (acfg_hdr +1);
    acfg_frame->id = 1;
    acfg_frame->type = ACFG_PKT_TYPE_GPR;
    acfg_frame->length = len;
    acfg_frame->nss = nss;
    acfg_frame->preamble = preamble;
    acfg_frame->mcs = mcs;
    acfg_frame->period = period;

    memcpy((acfg_frame + 1), pkt_buf, len);

    /* send the msg buffer to the driver */
    written = write(acfg_driver_sock, ((char *)buffer),
                    sizeof(struct nlmsghdr) + sizeof(struct acfg_offchan_hdr) +
                    sizeof(struct acfg_offchan_tx_hdr) + len);
    if ( written < len + sizeof(struct acfg_offchan_hdr) + sizeof(struct acfg_offchan_tx_hdr)) {
        acfg_log_errstr("Partial write. Closing connection. Size: %d Written: %d\n", len + sizeof(struct acfg_offchan_hdr), written);
        close(acfg_driver_sock);
        acfg_driver_sock = -1;
    }
    close(acfg_driver_sock);
    return status;
}

/**
 * @brief send_gpr_cmd per VAP
 *
 * @param
 * @vap_name VAP interface
 * @command: 0 - disable, 1 - enable, 2 - printstats,3 - clearstats
 *
 * @return
 */
uint32_t
acfg_send_gpr_cmd(uint8_t  *vap_name, uint8_t command)
{
    uint32_t   status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t req = {.cmd = ACFG_REQ_SEND_GPR_CMD};
    acfg_gpr_cmd_param_t   *ptr;
    ptr = (acfg_gpr_cmd_param_t *)req.data;
    ptr->command = command;

    status = acfg_os_send_req(vap_name, &req);
    if (status != QDF_STATUS_SUCCESS) {
        acfg_log_errstr("%s: send_gpr_cmd failed! \n", vap_name);
        return QDF_STATUS_E_FAILURE;
    }

    return status;
}
#endif


#define TOTAL_FRAMES 20

uint32_t
acfg_send_raw_multi(uint8_t  *vap_name, uint8_t *pkt_buf, uint32_t len, uint8_t type, uint16_t chan, uint8_t nss, uint8_t preamble, uint8_t mcs, uint8_t retry, uint8_t power, uint16_t scan_dur)
{
    uint32_t  status = QDF_STATUS_SUCCESS;
    int acfg_driver_sock;
    int on = 16*1024 ;
    int return_status;
    struct sockaddr_nl src_addr;
    struct acfg_offchan_hdr *acfg_hdr;
    uint32_t written, recvlen;
    struct acfg_offchan_tx_hdr *acfg_frame;
    int total_len, i;

    total_len = TOTAL_FRAMES * (len + sizeof(struct acfg_offchan_tx_hdr)) + sizeof(struct acfg_offchan_hdr)
                                    + sizeof(struct nlmsghdr);
    if (total_len > MAX_PAYLOAD) {
        acfg_log_errstr("%s: Bad length\n", __FUNCTION__);
        return -1;
    }
    if (strlen((char*)vap_name) >= ACFG_MAX_IFNAME)
        return -1;

    /* create a netlink socket */
    acfg_driver_sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ACFG);
    if (acfg_driver_sock < 0) {
        acfg_log_errstr("socket errno=%d\n", acfg_driver_sock);
        return acfg_driver_sock;
    }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    src_addr.nl_groups = 0;

    return_status = setsockopt(acfg_driver_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(return_status < 0) {
        acfg_log_errstr("nl socket option failed\n");
        close(acfg_driver_sock);
        return return_status;
    }
    return_status = bind(acfg_driver_sock, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(return_status < 0) {
        acfg_log_errstr("BIND errno=%d\n", return_status);
        close(acfg_driver_sock);
        return return_status;
    }

    nlh = (struct nlmsghdr *)buffer;
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(*acfg_hdr) + len);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    acfg_hdr = NLMSG_DATA(nlh);
    acfg_os_strcpy((uint8_t *)acfg_hdr->vap_name, (uint8_t *)vap_name, ACFG_MAX_IFNAME);

    acfg_hdr->msg_type = type;
    acfg_hdr->msg_length = len;
    acfg_hdr->channel = chan;
    acfg_hdr->scan_dur = scan_dur;
    acfg_hdr->num_frames = TOTAL_FRAMES;

    acfg_frame = (struct acfg_offchan_tx_hdr *) (acfg_hdr + 1);

    for (i = 0; i < acfg_hdr->num_frames; i++) {
        acfg_frame->id = 1;
        acfg_frame->type = type;
        acfg_frame->length = len;
        acfg_frame->nss = nss;
        acfg_frame->preamble = preamble;
        acfg_frame->mcs = mcs;
        acfg_frame->retry = retry;
        acfg_frame->power = power;

        memcpy((acfg_frame + 1), pkt_buf, len);

        acfg_frame = (struct acfg_offchan_tx_hdr *) ((char*)(acfg_frame + 1) + len);
    }
    /* Check if buffer size exceed the maximum capacity */
    if(sizeof(struct nlmsghdr) + total_len > sizeof(buffer)) {
        acfg_log_errstr("Memory limit of buffer exceeeded. Closing connection. \n");
        close(acfg_driver_sock);
        return -1;
    }
    /* send the msg buffer to the driver */
    written = write(acfg_driver_sock, ((char *)buffer), sizeof(struct nlmsghdr) + total_len);

    if (written < total_len + sizeof(struct nlmsghdr)) {
        // TBD : Need to write pending data if there is partial write
        acfg_log_errstr("Partial write. Closing connection. Size: %d Written: %d\n", len + sizeof(struct acfg_offchan_hdr), written);
        close(acfg_driver_sock);
        acfg_driver_sock = -1;
    }

 again:
    /* wait for a response from the driver */
	recvlen = recv(acfg_driver_sock, buffer, MAX_PAYLOAD, 0);
	if( recvlen <= 0 ) {
        acfg_log_errstr("Nothing to receive\n");
	} else {
        struct acfg_offchan_resp * acfg_resp;
        acfg_resp = (struct acfg_offchan_resp *) ((char *)buffer + sizeof(struct nlmsghdr));
        if (acfg_resp->hdr.msg_type == ACFG_CHAN_FOREIGN) {
            acfg_log_errstr("Foreign channel\n");
            goto again;
        } else if (acfg_resp->hdr.msg_type == ACFG_CHAN_HOME) {
            acfg_log_errstr("Home channel\n");
            goto again;
        } else {
            acfg_log_errstr("Tx status: %d\n", acfg_resp->hdr.msg_type);
            if (acfg_resp->hdr.msg_type != ACFG_PKT_STATUS_ERROR) {
                acfg_log_errstr("idx - Status\n");
                for (i = 0; i < acfg_resp->hdr.num_frames; i++)
                    acfg_log_errstr(" %d  -  %d\n", i, acfg_resp->status[i].status);
                acfg_log_errstr("\n");
            }
        }
    }
    return status;
}

uint32_t
acfg_send_raw_cancel(uint8_t  *vap_name)
{
    uint32_t  status = QDF_STATUS_SUCCESS;
    int acfg_driver_sock;
    int on = 16*1024 ;
    int return_status;
    struct sockaddr_nl src_addr;
    struct acfg_offchan_hdr *acfg_hdr;
    uint32_t written;

    if (strlen((char*)vap_name) >= ACFG_MAX_IFNAME)
        return -1;

    /* create a netlink socket */
    acfg_driver_sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ACFG);
    if (acfg_driver_sock < 0) {
        acfg_log_errstr("socket errno=%d\n", acfg_driver_sock);
        return acfg_driver_sock;
    }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    src_addr.nl_groups = 0;

    return_status = setsockopt(acfg_driver_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(return_status < 0) {
        acfg_log_errstr("nl socket option failed\n");
        close(acfg_driver_sock);
        return return_status;
    }
    return_status = bind(acfg_driver_sock, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(return_status < 0) {
        acfg_log_errstr("BIND errno=%d\n", return_status);
        close(acfg_driver_sock);
        return return_status;
    }

    nlh = (struct nlmsghdr *)buffer;
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(*acfg_hdr));
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    acfg_hdr = NLMSG_DATA(nlh);
    acfg_os_strcpy((uint8_t *)acfg_hdr->vap_name, (uint8_t *)vap_name, ACFG_MAX_IFNAME);

    acfg_hdr->msg_type = ACFG_CMD_CANCEL;

    /* send the msg buffer to the driver */
    written = write(acfg_driver_sock, ((char *)buffer),
                    sizeof(struct nlmsghdr) + sizeof(struct acfg_offchan_hdr));
    if ( written < sizeof(struct acfg_offchan_hdr)) {
        // TBD : Need to write pending data if there is partial write
        acfg_log_errstr("Partial write. Closing connection. Size: %d Written: %d\n", sizeof(struct acfg_offchan_hdr), written);
        close(acfg_driver_sock);
        acfg_driver_sock = -1;
    }

    return status;
}

static int get_bwmode_offset(char *param_str, uint8_t *bw_mode, uint8_t *sec_chan_offset)
{
    uint8_t ret;
    *sec_chan_offset = ACFG_SEC_CHAN_OFFSET_NA;

    if (strcmp ("40-", param_str) == 0) {
        *bw_mode = ACFG_OFFCHAN_BANDWIDTH_40MHZ;
        *sec_chan_offset = ACFG_SEC_CHAN_OFFSET_BELOW;
    } else if (strcmp ("40+", param_str) == 0) {
        *bw_mode = ACFG_OFFCHAN_BANDWIDTH_40MHZ;
        *sec_chan_offset = ACFG_SEC_CHAN_OFFSET_ABOVE;
    } else {
         ret = strtol(param_str, NULL, 10);

         switch(ret) {

             case 20: *bw_mode = ACFG_OFFCHAN_BANDWIDTH_20MHZ;
                       break;
             case 40: *bw_mode = ACFG_OFFCHAN_BANDWIDTH_40MHZ;
                       break;
             case 80: *bw_mode = ACFG_OFFCHAN_BANDWIDTH_80MHZ;
                       break;
             case 160: *bw_mode = ACFG_OFFCHAN_BANDWIDTH_160MHZ;
                       break;
             default:
                       return -1;
          }
    }
    return 0;
}

uint32_t
acfg_offchan_rx(uint8_t  *vap_name, uint16_t chan, uint16_t scan_dur, char *bw_mode_params)
{
    uint32_t  status = QDF_STATUS_SUCCESS;
    int acfg_driver_sock;
    int on = 16*1024 ;
    int return_status;
    struct sockaddr_nl src_addr;
    struct acfg_offchan_hdr *acfg_hdr;
    uint32_t written, recvlen;
    uint8_t bw_mode = 0;
    uint8_t sec_chan_offset = 0;

    if (strlen((char*)vap_name) >= ACFG_MAX_IFNAME)
        return -1;

    if (bw_mode_params) {
        return_status = get_bwmode_offset(bw_mode_params, &bw_mode, &sec_chan_offset);
        if (return_status < 0) {
            acfg_log_errstr("Invalid Bandwidth\n");
            return return_status;
        }
    }

    /* create a netlink socket */
    acfg_driver_sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ACFG);
    if (acfg_driver_sock < 0) {
        acfg_log_errstr("socket errno=%d\n", acfg_driver_sock);
        return acfg_driver_sock;
    }
    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK;
    src_addr.nl_pid = getpid();  /* self pid */
    /* interested in group 1<<0 */
    src_addr.nl_groups = 0;

    return_status = setsockopt(acfg_driver_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if(return_status < 0) {
        acfg_log_errstr("nl socket option failed\n");
        close(acfg_driver_sock);
        return return_status;
    }
    return_status = bind(acfg_driver_sock, (struct sockaddr*)&src_addr, sizeof(src_addr));
    if(return_status < 0) {
        acfg_log_errstr("BIND errno=%d\n", return_status);
        close(acfg_driver_sock);
        return return_status;
    }

    nlh = (struct nlmsghdr *)buffer;
    nlh->nlmsg_len = NLMSG_SPACE(sizeof(*acfg_hdr));
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    acfg_hdr = NLMSG_DATA(nlh);
    acfg_os_strcpy((uint8_t *)acfg_hdr->vap_name, (uint8_t *)vap_name, ACFG_MAX_IFNAME);

    acfg_hdr->msg_type = ACFG_CMD_OFFCHAN_RX;
    acfg_hdr->msg_length = 0;
    acfg_hdr->channel = chan;
    acfg_hdr->scan_dur = scan_dur;
    acfg_hdr->bw_mode = bw_mode;
    acfg_hdr->sec_chan_offset =
                sec_chan_offset;

    /* send the msg buffer to the driver */
    written = write(acfg_driver_sock, ((char *)buffer),
                    sizeof(struct nlmsghdr) + sizeof(struct acfg_offchan_hdr));
    if ( written < sizeof(struct acfg_offchan_hdr)) {
        // TBD : Need to write pending data if there is partial write
        acfg_log_errstr("Partial write. Closing connection. Size: %d Written: %d\n", sizeof(struct acfg_offchan_hdr), written);
        close(acfg_driver_sock);
        acfg_driver_sock = -1;
    }

    /* wait for a response from the driver */
	recvlen = recv(acfg_driver_sock, buffer, MAX_PAYLOAD, 0);
	if( recvlen <= 0 ) {
        acfg_log_errstr("Nothing to receive\n");
	} else {
        acfg_offchan_stat_t *offchan_stat;
        acfg_hdr = (struct acfg_offchan_hdr *) ((char *)buffer + sizeof(struct nlmsghdr));
        offchan_stat = (acfg_offchan_stat_t *) ((char *)buffer + sizeof(struct nlmsghdr) + sizeof(struct acfg_offchan_hdr));
        printf("status %d noise floor %d\n", acfg_hdr->msg_type, offchan_stat->noise_floor);
        printf("Dwell time %dus Home to Foreign switch time %dus Foreign to Home switch time %dus\n",
                offchan_stat->dwell_time, offchan_stat->chanswitch_time_htof, offchan_stat->chanswitch_time_ftoh);
    }
    return status;
}

/**
 * @brief Enable amsdu per TID
 *
 * @param radio_name
 * @param amsdutidmask
 *
 * @return
 */
uint32_t
acfg_enable_amsdu(uint8_t             *radio_name,
        uint16_t            amsdutidmask
)
{
    uint32_t   status = QDF_STATUS_SUCCESS;

    status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_ENABLE_AMSDU, amsdutidmask);

    return status;
}

/**
 * @brief Enable ampdu per TID
 *
 * @param radio_name
 * @param ampdutidmask
 *
 * @return
 */
uint32_t
acfg_enable_ampdu(uint8_t             *radio_name,
        uint16_t            ampdutidmask
)
{
    uint32_t   status = QDF_STATUS_SUCCESS;

    status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_ENABLE_AMPDU, ampdutidmask);

    return status;
}

/**
 * @brief set basic & supported rates in beacon,
 * and use lowest basic rate as mgmt mgmt/bcast/mcast rates by default.
 * target_rates: an array of supported rates with bit7 set for basic rates.
 * num_of_rates: number of rates in the array
*/
uint32_t
acfg_set_op_support_rates(uint8_t  *radio_name, uint8_t *target_rates, uint32_t num_of_rates)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_SET_OP_SUPPORT_RATES};
    acfg_rateset_t  *rs;
    uint32_t      i = 0, j = 0;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    rs = (acfg_rateset_t *)req.data;
    if(num_of_rates > ACFG_MAX_RATE_SIZE){
        num_of_rates = ACFG_MAX_RATE_SIZE;
    }

    /* Check if any two rates are same */
    for(i=0;i<num_of_rates;i++){
        for(j=i+1;j<num_of_rates;j++){
            if(j == num_of_rates){
                break;
            }
            if((target_rates[i]&ACFG_RATE_VAL) == (target_rates[j]&ACFG_RATE_VAL)){
                acfg_log_errstr("%s failed! Same rates found: %d,%d !\n",__FUNCTION__, target_rates[i], target_rates[j]);
                return status;
            }
        }
    }

    rs->rs_nrates = num_of_rates;
    memcpy(rs->rs_rates, target_rates, num_of_rates);

    status = acfg_os_send_req(radio_name, &req);

    return status;
}

/**
 * @brief get supported legacy rates of the specified phymode for the radio
 * target_rates: return an array of supported rates with bit7 set for basic rates.
 * phymode : phymode
*/
uint32_t
acfg_get_radio_supported_rates(uint8_t  *radio_name, acfg_rateset_t *target_rates, acfg_phymode_t phymode)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_RADIO_SUPPORTED_RATES};
    acfg_rateset_phymode_t  *rs_phymode;
    acfg_rateset_t  *rs;
    uint8_t i=0;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    rs_phymode = (acfg_rateset_phymode_t *)req.data;
    rs_phymode->phymode = phymode;
    rs = &(rs_phymode->rs);
    if(rs->rs_nrates > ACFG_MAX_RATE_SIZE){
        rs->rs_nrates = ACFG_MAX_RATE_SIZE;
    }

    status = acfg_os_send_req(radio_name, &req);
    if(status!=QDF_STATUS_SUCCESS || rs->rs_nrates==0){
        acfg_log_errstr("%s failed!\n",__FUNCTION__);
        return QDF_STATUS_E_FAILURE;
    }

    for(i=0;i<rs->rs_nrates;i++){
        target_rates->rs_rates[i] = rs->rs_rates[i];
    }
    target_rates->rs_nrates = rs->rs_nrates;

    return QDF_STATUS_SUCCESS;
}

/**
 * @brief get supported legacy rates from BEACON IE
 * target_rates: return an array of supported rates with bit7 set for basic rates.
*/
uint32_t
acfg_get_beacon_supported_rates(uint8_t  *vap_name, acfg_rateset_t *target_rates)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_BEACON_SUPPORTED_RATES};
    acfg_rateset_t  *rs;
    uint8_t i=0;

    if (acfg_os_check_str(vap_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    rs = (acfg_rateset_t *)req.data;
    rs->rs_nrates = sizeof(rs->rs_rates)/sizeof(uint8_t);
    if(rs->rs_nrates > ACFG_MAX_RATE_SIZE){
        rs->rs_nrates = ACFG_MAX_RATE_SIZE;
    }

    status = acfg_os_send_req(vap_name, &req);
    if(status!=QDF_STATUS_SUCCESS || rs->rs_nrates==0){
        acfg_log_errstr("%s failed, status=%d!\n",__FUNCTION__,status);
        return QDF_STATUS_E_FAILURE;
    }

    for(i=0;i<rs->rs_nrates;i++){
        target_rates->rs_rates[i] = rs->rs_rates[i];
    }
    target_rates->rs_nrates = rs->rs_nrates;

    return QDF_STATUS_SUCCESS;
}


/**
 * @brief Set management/action frame retry limit
 *
 * @param
 * @radio_name: Physical radio interface
 * @limit:      Management/action frame retry limit
 *
 * @return
 */
uint32_t
acfg_set_mgmt_retry_limit(uint8_t *radio_name, uint8_t limit)
{
    uint32_t   status = QDF_STATUS_SUCCESS;
    status = acfg_set_radio_param(radio_name, ACFG_PARAM_RADIO_MGMT_RETRY_LIMIT, limit);
    return status;
}

/**
 * @brief Get management/action frame retry limit
 *
 * @param
 * @radio_name: Physical radio interface
 * @limit:      Management/action frame retry limit
 *
 * @return
 */
uint32_t
acfg_get_mgmt_retry_limit(uint8_t *radio_name, uint8_t *limit)
{
    uint32_t status = QDF_STATUS_E_FAILURE;
    uint32_t val;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_NOENT;

    status = acfg_get_radio_param(radio_name, ACFG_PARAM_RADIO_MGMT_RETRY_LIMIT, &val);
    if (status == QDF_STATUS_SUCCESS) {
        *limit = (uint8_t)val;
    }

    return status;
}

/**
 * @brief set 11AX bss color
 * bsscolor: BSS color value range {0...63}
 * override: to override, value 0/1
*/
uint32_t
acfg_set_bss_color(uint8_t *radio_name, uint32_t bsscolor, uint32_t override)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_SET_BSS_COLOR};
    acfg_bss_color_t *bss_color_t;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    bss_color_t = (acfg_bss_color_t *)req.data;
    bss_color_t->bsscolor = bsscolor;
    bss_color_t->override = override;

    status = acfg_os_send_req(radio_name, &req);
    return status;
}


/**
 * @brief get 11AX bss color
 * bsscolor: BSS color value range {0...63}
*/
uint32_t
acfg_get_bss_color(uint8_t *radio_name, uint32_t *bsscolor)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_BSS_COLOR};
    acfg_bss_color_t *bss_color_t;

    if (acfg_os_check_str(radio_name, ACFG_MAX_IFNAME))
        return QDF_STATUS_E_INVAL;

    status = acfg_os_send_req(radio_name, &req);
    if(status!=QDF_STATUS_SUCCESS){
        acfg_log_errstr("%s failed, status=%d!\n",__FUNCTION__,status);
        return QDF_STATUS_E_FAILURE;
    }

    bss_color_t = (acfg_bss_color_t *)req.data;
    *bsscolor = bss_color_t->bsscolor;

    return QDF_STATUS_SUCCESS;
}


/**
 * @brief set 11AX MU EDCA param ecwmin
 * ac: AC
 * value: value
*/
uint32_t
acfg_set_muedca_ecwmin(uint8_t *vap, uint32_t ac, uint32_t value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_SET_MUEDCA_ECWMIN};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;
    edca_param->val = value;

    status = acfg_os_send_req(vap, &req);
    return status;
}

/**
 * @brief get 11AX MU EDCA param ecwmin
 * ac: AC
 * value: return value
*/
uint32_t
acfg_get_muedca_ecwmin(uint8_t *vap, uint32_t ac, uint32_t *value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_MUEDCA_ECWMIN};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;

    status = acfg_os_send_req(vap, &req);
    if(status!=QDF_STATUS_SUCCESS){
        acfg_log_errstr("%s failed, status=%d!\n",__FUNCTION__,status);
        return QDF_STATUS_E_FAILURE;
    }
    *value = edca_param->val;

    return status;
}


/**
 * @brief set 11AX MU EDCA param
 * ac: AC
 * value: value
*/
uint32_t
acfg_set_muedca_ecwmax(uint8_t *vap, uint32_t ac, uint32_t value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_SET_MUEDCA_ECWMAX};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;
    edca_param->val = value;

    status = acfg_os_send_req(vap, &req);
    return status;
}

/**
 * @brief get 11AX MU EDCA param
 * ac: AC
 * value: return value
*/
uint32_t
acfg_get_muedca_ecwmax(uint8_t *vap, uint32_t ac, uint32_t *value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_MUEDCA_ECWMAX};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;

    status = acfg_os_send_req(vap, &req);
    if(status!=QDF_STATUS_SUCCESS){
        acfg_log_errstr("%s failed, status=%d!\n",__FUNCTION__,status);
        return QDF_STATUS_E_FAILURE;
    }
    *value = edca_param->val;

    return status;
}


/**
 * @brief set 11AX MU EDCA param
 * ac: AC
 * value: value
*/
uint32_t
acfg_set_muedca_aifsn(uint8_t *vap, uint32_t ac, uint32_t value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_SET_MUEDCA_AIFSN};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;
    edca_param->val = value;

    status = acfg_os_send_req(vap, &req);
    return status;
}

/**
 * @brief get 11AX MU EDCA param
 * ac: AC
 * value: return value
*/
uint32_t
acfg_get_muedca_aifsn(uint8_t *vap, uint32_t ac, uint32_t *value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_MUEDCA_AIFSN};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;

    status = acfg_os_send_req(vap, &req);
    if(status!=QDF_STATUS_SUCCESS){
        acfg_log_errstr("%s failed, status=%d!\n",__FUNCTION__,status);
        return QDF_STATUS_E_FAILURE;
    }
    *value = edca_param->val;

    return status;
}

/**
 * @brief set 11AX MU EDCA param
 * ac: AC
 * value: value
*/
uint32_t
acfg_set_muedca_acm(uint8_t *vap, uint32_t ac, uint32_t value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_SET_MUEDCA_ACM};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;
    edca_param->val = value;

    status = acfg_os_send_req(vap, &req);
    return status;
}

/**
 * @brief get 11AX MU EDCA param
 * ac: AC
 * value: return value
*/
uint32_t
acfg_get_muedca_acm(uint8_t *vap, uint32_t ac, uint32_t *value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_MUEDCA_ACM};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;

    status = acfg_os_send_req(vap, &req);
    if(status!=QDF_STATUS_SUCCESS){
        acfg_log_errstr("%s failed, status=%d!\n",__FUNCTION__,status);
        return QDF_STATUS_E_FAILURE;
    }
    *value = edca_param->val;

    return status;
}

/**
 * @brief set 11AX MU EDCA param
 * ac: AC
 * value: value
*/
uint32_t
acfg_set_muedca_timer(uint8_t *vap, uint32_t ac, uint32_t value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_SET_MUEDCA_TIMER};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;
    edca_param->val = value;

    status = acfg_os_send_req(vap, &req);
    return status;
}

/**
 * @brief get 11AX MU EDCA param
 * ac: AC
 * value: return value
*/
uint32_t
acfg_get_muedca_timer(uint8_t *vap, uint32_t ac, uint32_t *value)
{
    uint32_t      status = QDF_STATUS_E_FAILURE;
    acfg_os_req_t   req = {.cmd = ACFG_REQ_GET_MUEDCA_TIMER};
    acfg_muedca_param_t *edca_param;

    edca_param = (acfg_muedca_param_t *)req.data;
    edca_param->ac = ac;

    status = acfg_os_send_req(vap, &req);
    if(status!=QDF_STATUS_SUCCESS){
        acfg_log_errstr("%s failed, status=%d!\n",__FUNCTION__,status);
        return QDF_STATUS_E_FAILURE;
    }
    *value = edca_param->val;

    return status;
}

