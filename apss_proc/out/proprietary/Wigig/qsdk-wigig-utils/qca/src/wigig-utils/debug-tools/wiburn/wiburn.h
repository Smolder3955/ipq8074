/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _WIBURN_H_
#define _WIBURN_H_

extern bool g_debug;
extern volatile bool g_exit;

#ifdef _WINDOWS
#include <windows.h>

#define STRTOULL _strtoui64

typedef unsigned __int8 u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;
#if (defined(_MSC_VER) && (_MSC_VER < 1900))
typedef __int8 int8_t;
#endif
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;

#else //#ifdef _WINDOWS

#define STRTOULL strtoull

#include <sys/types.h>
typedef u_int8_t u_int8_t;
typedef u_int16_t u_int16_t;
typedef u_int32_t u_int32_t;
typedef u_int64_t u_int64_t;
typedef int8_t int8_t;
typedef int16_t int16_t;
typedef int32_t int32_t;
typedef int64_t int64_t;
#endif  //#ifdef _WINDOWS

typedef u_int32_t ADDRESS32;

typedef u_int8_t BYTE;
typedef u_int32_t IMAGE;


#define PAGE 256
#define FLASH_PAGE_MASK 0xffffff00
#define SUB_SECTOR (16*PAGE) // 4KB
#define SUB_SECTOR_MASK 0xfffff000
#define SECTOR (16*SUB_SECTOR) // 64KB

#define ERR printf
#define DBG printf
#define INFO printf

void EXIT( int val );

void print_buffer(const void *buffer, int length);
#endif //#ifndef _WIBURN_H_
