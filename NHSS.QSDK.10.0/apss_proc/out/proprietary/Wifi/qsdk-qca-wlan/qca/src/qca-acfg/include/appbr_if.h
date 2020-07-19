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

#ifndef __APPBR_IF_H__

#define __APPBR_IF_H__

#include <appbr_types.h>
//#include <acfg_api_types.h>
#include <bypass_types.h>

/**
 * @brief Open connection for downlink (send messages) communication
 *
 * @param app_id Application Identifier
 *
 * @return
 */
appbr_status_t
appbr_if_open_dl_conn(uint32_t app_id);

/**
 * @brief Close downlink connection
 */
void
appbr_if_close_dl_conn(void);

/**
 * @brief Open connection for uplink (recv messages) communication
 *
 * @param app_id Application Identifier
 *
 * @return
 */
appbr_status_t
appbr_if_open_ul_conn(uint32_t app_id);

/**
 * @brief Close uplink connection
 */
void
appbr_if_close_ul_conn(void);

/**
 * @brief Send a command to the Peer Application
 *
 * @param app_id Application ID
 * @param buf    Command Buffer Pointer
 * @param size   Command Size
 *
 * @return
 */
appbr_status_t
appbr_if_send_cmd_remote(uint32_t app_id, void *buf, uint32_t size);

/**
 * @brief   Wait for response from Peer Application
 *
 * @param buf       Response Buffer Pointer
 * @param size      Response Size
 * @param timeout   Time to wait for Response
 *
 * @return
 */
appbr_status_t
appbr_if_wait_for_response(void *buf, uint32_t size, uint32_t timeout);

#endif
