/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

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

#ifndef __A_BASE_TYPES_H
#define __A_BASE_TYPES_H



/** 
 * @brief The basic data types
 */
typedef __a_uint8_t     a_uint8_t; /**< 1 Byte */
typedef __a_uint16_t    a_uint16_t;/**< 2 Bytes */
typedef __a_uint32_t    a_uint32_t;/**< 4 Bytes */
typedef __a_uint64_t    a_uint64_t;/**< 4 Bytes */

typedef __a_int8_t      a_int8_t; /**< 1 Byte */
typedef __a_int16_t     a_int16_t;/**< 2 Bytes */
typedef __a_int32_t     a_int32_t;/**< 4 Bytes */
typedef __a_int64_t     a_int64_t;/**< 4 Bytes */

typedef __a_char_t      a_char_t;

enum a_bool {
    A_TRUE  = 0,
    A_FALSE = 1
};
typedef a_uint8_t   a_bool_t;/**< 1 Byte */


/** 
 * @brief Generic status for the API's
 */
enum {
    A_STATUS_OK = 0,
    A_STATUS_FAILED, 
    A_STATUS_ENOENT,
    A_STATUS_ENOMEM,
    A_STATUS_EINVAL,
    A_STATUS_EINPROGRESS,
    A_STATUS_ENOTSUPP,
    A_STATUS_EBUSY,
    A_STATUS_E2BIG,
    A_STATUS_EAGAIN,
    A_STATUS_ENOSPC,
    A_STATUS_EADDRNOTAVAIL,
    A_STATUS_ENXIO,
    A_STATUS_ENETDOWN,
    A_STATUS_EFAULT,
    A_STATUS_EIO,
	A_STATUS_ENETRESET,
	A_STATUS_EEXIST,
	A_STATUS_SIG    /* Exit due to received SIGINT */
};
typedef a_uint32_t  a_status_t;

#endif
