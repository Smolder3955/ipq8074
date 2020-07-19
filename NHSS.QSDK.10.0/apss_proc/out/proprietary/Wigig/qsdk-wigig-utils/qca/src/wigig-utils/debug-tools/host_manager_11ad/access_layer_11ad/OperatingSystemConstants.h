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

#pragma once

#ifdef _WINDOWS
/* Added here so windows.h will not include winsock.h that is interfering with winsock2.h */
#define _WINSOCKAPI_
#include <windows.h>

typedef unsigned __int8 u_int8_t;
typedef unsigned __int16 u_int16_t;
typedef unsigned __int32 u_int32_t;
typedef unsigned __int64 u_int64_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

#else

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#include <string>

#define __INLINE   __inline

typedef uint8_t         BYTE;
typedef uint16_t        WORD;
typedef uint32_t        DWORD;

typedef unsigned char   UCHAR;
typedef unsigned int    UINT;
typedef char            CHAR;
typedef long            LONG;
typedef unsigned short  USHORT;
typedef unsigned long   ULONG;
typedef ULONG*          ULONG_PTR;

typedef char            TCHAR;

typedef uint8_t         u_int8_t;
typedef uint16_t        u_int16_t;
typedef uint32_t        u_int32_t;
typedef uint64_t        u_int64_t;

typedef const TCHAR     *LPCTSTR;
typedef TCHAR           *LPTSTR;
typedef CHAR            *LPSTR;
typedef const CHAR      *LPCSTR;

typedef DWORD           HANDLE;

typedef int             BOOL;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <strings.h>

#define WLCT_MSEC_IN_SEC 1000

#define USES_CONVERSION

#define T2A(x) (x)

#define _tcscpy_s(x, y, z) snprintf((x), (y), "%s", (z))
#define _tcslen            strlen
#define _stprintf_s        snprintf
#define _snprintf          snprintf
//#define sprintf_s          sprintf
//#define _stprintf          sprintf
#define sscanf_s           sscanf
#define _tcstok_s          strtok_r
#define _tcsstr            strstr
#define _tcsicmp           strcasecmp
#define _vsntprintf        vsnprintf
#define _tfopen            fopen

#endif
