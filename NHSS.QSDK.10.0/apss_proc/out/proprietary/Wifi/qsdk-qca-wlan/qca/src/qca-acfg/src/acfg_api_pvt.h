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

#ifndef __ACFG_API_PVT_H
#define __ACFG_API_PVT_H

#include <stdint.h>
#include <qdf_types.h>
#include <acfg_api_types.h>
#include <acfg_api.h>

#define ACFG_CONF_FILE "/etc/acfg_common.conf"
#define ACFG_APP_CTRL_IFACE "/tmp/acfg-app"

typedef struct {
    uint8_t new_vap_idx[ACFG_MAX_VAPS];
    uint8_t cur_vap_idx[ACFG_MAX_VAPS];
    uint8_t num_vaps;
} acfg_wlan_profile_vap_list_t;

typedef struct vap_list {
    char iface[ACFG_MAX_VAPS][ACFG_MAX_IFNAME];
    int num_iface;
} acfg_vap_list_t;


/**
 * @brief Send Request Data in a OS specific way
 *
 * @param hdl
 * @param req (Request Structure)
 *
 * @return
 */
uint32_t
acfg_os_send_req(uint8_t  *ifname, acfg_os_req_t  *req);

uint32_t
acfg_os_check_str(uint8_t *src, uint32_t maxlen);

uint32_t
acfg_os_strcpy(uint8_t  *dst, uint8_t *src, uint32_t  maxlen);

uint32_t
acfg_os_cmp_str(uint8_t *str1, uint8_t *str2, uint32_t maxlen) ;

uint32_t
acfg_log(uint8_t *msg);

uint8_t
acfg_mhz2ieee(uint32_t);

uint32_t
acfg_hostapd_modify_bss(acfg_wlan_profile_vap_params_t *vap_params,
        acfg_wlan_profile_vap_params_t *cur_vap_params,
        int8_t *sec);

uint32_t
acfg_hostapd_delete_bss(acfg_wlan_profile_vap_params_t *vap_params);

uint32_t
acfg_hostapd_add_bss(acfg_wlan_profile_vap_params_t *vap_params, int8_t *sec);

uint32_t
acfg_get_iface_list(acfg_vap_list_t *list, int *count);

int
acfg_get_ctrl_iface_path(char *filename, char *hapd_ctrl_iface_dir,
        char *wpa_supp_ctrl_iface_dir);
int acfg_ctrl_req(uint8_t *ifname, char *cmd, size_t cmd_len, char *replybuf,
        uint32_t *reply_len, acfg_opmode_t opmode);
void
acfg_update_wps_config_file(uint8_t *ifname, char *prefix, char *data, int len);

void
acfg_update_wps_dev_config_file(acfg_wlan_profile_vap_params_t *vap_params, int force_update);

void acfg_set_wps_default_config(acfg_wlan_profile_vap_params_t *vap_params);

void acfg_set_hs_iw_vap_param(acfg_wlan_profile_vap_params_t *vap_params);

uint32_t
acfg_wlan_iface_up(uint8_t  *ifname, acfg_wlan_profile_vap_params_t *vap_params);

uint32_t
acfg_wlan_iface_down(uint8_t *ifname, acfg_wlan_profile_vap_params_t *vap_params);

void
acfg_rem_wps_config_file(uint8_t *ifname);

void acfg_reset_errstr(void);

void _acfg_log_errstr(const char *fmt, ...);
void _acfg_print(const char *fmt, ...);

//Prints all the debug messages
#define ACFG_DEBUG 0
//Prints only the error messages
#define ACFG_DEBUG_ERROR 0

#if ACFG_DEBUG
#undef ACFG_DEBUG_ERROR
#define ACFG_DEBUG_ERROR 1
#define acfg_print(fmt, ...) _acfg_print(fmt, ##__VA_ARGS__)
#else
#define acfg_print(fmt, ...)
#endif

#if ACFG_DEBUG_ERROR
#define acfg_log_errstr(fmt, ...) _acfg_print(fmt, ##__VA_ARGS__)
#else
#define acfg_log_errstr(fmt, ...) _acfg_log_errstr(fmt, ##__VA_ARGS__)
#endif

#define ACFG_RATE_BASIC            (0x80)
#define ACFG_RATE_VAL              (0x7f)

#define MAX_PAYLOAD 8192

#endif
