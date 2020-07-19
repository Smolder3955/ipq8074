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

#include "wlct_os.h"

class Util
{
public:
    static bool getFileVersionInfo(const TCHAR *fileNameFillPath, int &mjr, int &mnr, int &mnt, int &build);
    static BOOL IsNumeric(LPCTSTR pszString, BOOL bIgnoreColon);
    static DWORD ReadRegistrySettings(LPCTSTR lpszRegistryPath, LPCTSTR valName);
    static DWORD WriteRegistrySettings(LPCTSTR lpszRegistryPath, LPCTSTR valName, DWORD val);
    static int   IsWlctDevicePlugin();
//    static bool Util::GetWlstPluggedInDevice();
    static bool ICHDisableEnable();
    static bool GetPciControllerDevicePath(TCHAR* devicePath);

    // perfrom atomic change value, to acquire resource!
    // if VAR != FLAG_VALUE, then set VAR to FLAG_VALUE.
    // loop until timeout (milisec). return success in acheiving resource
    // Assumes that FLAG_VALUE should be diffrent then current value stored in VAR
    // turn keepWaiting on to wait infinite

    static BOOL timedResourceInterLockExchange ( LONG* var, LONG flag_value, int timeout, bool keepWaiting );

    static BOOL Is64BitWindows();

private:
    Util(void);
    ~Util(void);
};

#ifdef _WINDOWS
#include <setupapi.h>
#include <cfgmgr32.h>

typedef HKEY (__stdcall SETUPDIOPENDEVREGKEY)(HDEVINFO, PSP_DEVINFO_DATA, DWORD, DWORD, DWORD, REGSAM);
typedef BOOL (__stdcall SETUPDICLASSGUIDSFROMNAME)(LPCTSTR, LPGUID, DWORD, PDWORD);
typedef BOOL (__stdcall SETUPDIDESTROYDEVICEINFOLIST)(HDEVINFO);
typedef BOOL (__stdcall SETUPDIENUMDEVICEINFO)(HDEVINFO, DWORD, PSP_DEVINFO_DATA);
typedef HDEVINFO (__stdcall SETUPDIGETCLASSDEVS)(LPGUID, LPCTSTR, HWND, DWORD);
typedef BOOL (__stdcall SETUPDIGETDEVICEREGISTRYPROPERTY)(HDEVINFO, PSP_DEVINFO_DATA, DWORD, PDWORD, PBYTE, DWORD, PDWORD);
typedef DWORD (__stdcall CM_GET_DEVICE_ID)(DEVINST, PTCHAR, ULONG, ULONG);
#endif
