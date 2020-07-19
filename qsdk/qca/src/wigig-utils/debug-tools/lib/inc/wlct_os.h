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

#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
#include <assert.h>

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
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

typedef DWORD wlct_os_err_t;

#define WLCT_OS_ERROR_SUCCESS              ERROR_SUCCESS
#define WLCT_OS_ERROR_NOT_SUPPORTED        ERROR_NOT_SUPPORTED
#define WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED ERROR_CALL_NOT_IMPLEMENTED
#define WLCT_OS_ERROR_GEN_FAILURE          ERROR_GEN_FAILURE
#define WLCT_OS_ERROR_NOT_ENOUGH_MEMORY    ERROR_NOT_ENOUGH_MEMORY
#define WLCT_OS_ERROR_NO_SUCH_ENTRY        ERROR_FILE_NOT_FOUND
#define WLCT_OS_ERROR_OPEN_FAILED          ERROR_OPEN_FAILED

#define WLCT_ASSERT assert

#define __TRY __try

#define __EXCEPT __except

#define sleep_ms Sleep

#define WlctGetLastError GetLastError

#else

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

#include <string>

#define WLCT_ASSERT assert

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

typedef int wlct_os_err_t;

#define WLCT_OS_ERROR_SUCCESS              0
#define WLCT_OS_ERROR_NOT_SUPPORTED        -ENOTSUP
#define WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED -ENOSYS
#define WLCT_OS_ERROR_GEN_FAILURE          -EINVAL
#define WLCT_OS_ERROR_NOT_ENOUGH_MEMORY    -ENOMEM
#define WLCT_OS_ERROR_NO_SUCH_ENTRY        -ENOENT
#define WLCT_OS_ERROR_OPEN_FAILED          -EBADF

#define _T(x) (x)

#define __TRY if (1)
#define __EXCEPT(ignore) else if (0)

#define WLCT_MSEC_IN_SEC 1000

#define sleep_ms(msec) usleep(msec * WLCT_MSEC_IN_SEC)

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

#define WlctGetLastError() errno

#endif

#include "StlChar.h"


#define WLCT_UNREFERENCED_PARAM(x)  ((x) = (x))
