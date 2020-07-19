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

class IIoctlDev
{
public:
  IIoctlDev(const TCHAR* tchDeviceName)
  {
    WLCT_ASSERT(tchDeviceName != NULL);
    WLCT_ASSERT(sizeof(szInterfaceName) > _tcslen(tchDeviceName));
    _tcscpy_s(szInterfaceName, INTERFACE_NAME_LENGTH, tchDeviceName);
  }

  virtual ~IIoctlDev()
  {

  }

  virtual bool          IsOpened(void) = 0;

  virtual wlct_os_err_t Open() = 0;
  virtual wlct_os_err_t Ioctl(uint32_t Id,
                              const void *inBuf, uint32_t inBufSize,
                              void *outBuf, uint32_t outBufSize) = 0;
  virtual wlct_os_err_t DebugFS(char *FileName, void *dataBuf, DWORD dataBufLen, DWORD DebugFSFlags){
      //do something with params
    (void)FileName;
    (void)dataBuf;
    (void)dataBufLen;
    (void)DebugFSFlags;
    return -1;
  }
  virtual void          Close() = 0;

protected:
  static const size_t INTERFACE_NAME_LENGTH = 256;
  TCHAR szInterfaceName[INTERFACE_NAME_LENGTH];
};
