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

#include "flash_gateway.h"

#define ERR printf
#define DBG printf
#define INFO printf
//#define _EXIT_(val) {exit (val);}

extern bool g_debug;


typedef struct _FLSH_ACCSS_DLL_VERSION
{
    int major;
    int minor;
    int maintenance;
    int build;
    char fullFilePath[256];

}FLSH_ACCSS_DLL_VERSION;


// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the WLCTPCIACSS_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// WLCTPCIACSS_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.

#ifdef _WINDOWS
#ifdef FLSHACSS_EXPORTS
#define FLSHACSS_API __declspec(dllexport)
#else
#define FLSHACSS_API __declspec(dllimport)
#endif
#endif   // _WINDOWS

#if defined(__GNUC__)
#if defined(__i386)
#define FLSHACSS_API extern "C" __attribute__((cdecl))
#else
#define FLSHACSS_API extern "C"
#endif
#endif   // __GNUC__


FLSHACSS_API int wfa_create_flash_access_handler(DType devType, void* pDeviceAccess, void** pFlashAccess);
FLSHACSS_API int wfa_close_flash_access_handler(void* pFlashAccess);
FLSHACSS_API int wfa_read(void* pFlashAccess, BYTE *buffer, unsigned long offset, unsigned long size);
FLSHACSS_API int wfa_write(void* pFlashAccess, const BYTE *buffer, unsigned long offset, unsigned long size);
FLSHACSS_API int wfa_program(void* pFlashAccess);
FLSHACSS_API int wfa_erase(void* pFlashAccess);

FLSHACSS_API int wfa_run_command(void* pFlashAccss, UCHAR flash_cmd, FLASH_PHASE phase, bool bWrite, unsigned long addr, unsigned long ulSize);
FLSHACSS_API int wfa_set_data(void* pFlashAccss, const BYTE *buffer, unsigned long size);
FLSHACSS_API int wfa_get_data(void* pFlashAccss, BYTE *buffer, unsigned long size);
FLSHACSS_API int wfa_wait_done(void* pFlashAccss);
FLSHACSS_API int wfa_boot_done(void* pFlashAccss);
FLSHACSS_API bool wfa_get_lock(void* pFlashAccss);
FLSHACSS_API int wfa_force_lock(void* pFlashAccss);
FLSHACSS_API int wfa_release_lock(void* pFlashAccss);

