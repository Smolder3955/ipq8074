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

#ifndef _ACFG_WSUPP_TYPES_H
#define _ACFG_WSUPP_TYPES_H

#include "qdf_types.h"
#include "acfg_api_types.h"

typedef void acfg_wsupp_hdl_t;

/** network items that can be set */
enum acfg_wsupp_nw_item {
    ACFG_WSUPP_NW_SSID,
    ACFG_WSUPP_NW_PSK,
    ACFG_WSUPP_NW_KEY_MGMT,
    ACFG_WSUPP_NW_ENABLE,     /**< actually starts the network connection */
    ACFG_WSUPP_NW_PAIRWISE,
    ACFG_WSUPP_NW_GROUP,
    ACFG_WSUPP_NW_PROTO,
};

/** types of wps calls */
enum acfg_wsupp_wps_type {
    ACFG_WSUPP_WPS_PIN,
    ACFG_WSUPP_WPS_PBC,
    ACFG_WSUPP_WPS_REG,
    ACFG_WSUPP_WPS_ER_START,
    ACFG_WSUPP_WPS_ER_STOP,
    ACFG_WSUPP_WPS_ER_PIN,
    ACFG_WSUPP_WPS_ER_PBC,
    ACFG_WSUPP_WPS_ER_LEARN,
};

/** types of set calls */
enum acfg_wsupp_set_type {
    ACFG_WSUPP_SET_UUID,
    ACFG_WSUPP_SET_DEVICE_NAME,
    ACFG_WSUPP_SET_MANUFACTURER,
    ACFG_WSUPP_SET_MODEL_NAME,
    ACFG_WSUPP_SET_MODEL_NUMBER,
    ACFG_WSUPP_SET_SERIAL_NUMBER,
    ACFG_WSUPP_SET_DEVICE_TYPE,
    ACFG_WSUPP_SET_OS_VERSION,
    ACFG_WSUPP_SET_CONFIG_METHODS,
    ACFG_WSUPP_SET_SEC_DEVICE_TYPE,
    ACFG_WSUPP_SET_PERSISTENT_RECONNECT,
    ACFG_WSUPP_SET_COUNTRY,
};

#endif
