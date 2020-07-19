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

#include "wlct_cdev_shared.h"
#include "wlct_os.h"

#include <string>

class CWlctCDevFile
{
public:
  CWlctCDevFile();
  virtual ~CWlctCDevFile();

  virtual wlct_os_err_t Open(const char *fName, const char* interfaceName);
  virtual void          Close();
  virtual wlct_os_err_t Ioctl(void *dataBuf, DWORD dataBufLen, DWORD ioctlFlags);
  virtual wlct_os_err_t DebugFS(char *FileName, void *dataBuf, DWORD dataBufLen, DWORD DebugFSFlags)
  {
    //do something with params
    (void)FileName;
    (void)dataBuf;
    (void)dataBufLen;
    (void)DebugFSFlags;

    return WLCT_OS_ERROR_CALL_NOT_IMPLEMENTED;
  }

  bool          IsOpened() const
  {
    return (fd != INVALID_FD);
  }

protected:

  int  fd;
  static const int INVALID_FD;

private:

  std::string devFileName;

};

