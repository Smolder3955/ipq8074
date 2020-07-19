/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
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
 * Offload specific Opaque Data types.
 */
#ifndef _DEV_OL_DEFINES_H
#define _DEV_OL_DEFINES_H

/**
 * @brief Opaque handle of offload device struct.
 */
struct ol_ath_softc_net80211;
struct ol_ath_soc_softc;
/*Opaque pointer for SoC */
typedef struct ol_ath_soc_softc ol_ath_soc_softc_t;

/**
 * @brief Opaque handle of offload vap struct.
 */
struct ol_ath_vap_net80211;
typedef struct ol_ath_softc_net80211 *ol_avn_t;

/**
 * @brief Opaque handle of offload node struct.
 */
struct ol_ath_node_net80211;
typedef struct ol_ath_node_net80211 *ol_an_t;

typedef void *ol_soc_t;

#if 0
/**
 * @brief Opaque handle of wmi structure
 */
struct wmi_unified;
typedef struct wmi_unified *wmi_unified_t;

/**
 * @wmi_event_handler function prototype
 */
typedef int (*wmi_unified_event_handler) (ol_ath_soc_softc_t *soc, u_int8_t *event_buf,
                                           u_int32_t len);
#endif

#endif /* _DEV_OL_DEFINES_H */
