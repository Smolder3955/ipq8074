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

#include "wlct_os.h"
#include "WlctCDevFile.h"
#include "LoggerSupport.h"

extern "C" {
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
};

const int CWlctCDevFile::INVALID_FD = -1;

CWlctCDevFile::CWlctCDevFile(): fd(INVALID_FD)
{
}

CWlctCDevFile::~CWlctCDevFile()
{
  Close();
}

wlct_os_err_t CWlctCDevFile::Open(const char *fName, const char* interfaceName)
{
  //do something with params
  (void)interfaceName;
  wlct_os_err_t res = WLCT_OS_ERROR_GEN_FAILURE;

  WLCT_ASSERT(IsOpened() == false);
  WLCT_ASSERT(fName != NULL);

  devFileName = fName;

  LOG_MESSAGE_DEBUG("Char device file %s opening...", devFileName.c_str());

  fd = open(devFileName.c_str(), 0);
  if (INVALID_FD == fd)
  {
    res = errno;
    LOG_MESSAGE_ERROR("Failed to open file %s (%d)", devFileName.c_str(), res);
    goto Exit;
  }

  LOG_MESSAGE_DEBUG("Char device file opened: fd=%d", fd);

  res = WLCT_OS_ERROR_SUCCESS;

Exit:
  if (res != WLCT_OS_ERROR_SUCCESS)
  {
    Close();
  }

  return res;
}

void CWlctCDevFile::Close()
{
  if (fd != INVALID_FD)
  {
    close(fd);
    fd = INVALID_FD;
  }

  devFileName.erase();
}

wlct_os_err_t CWlctCDevFile::Ioctl(void *dataBuf,
                                   DWORD dataBufLen,
                                   DWORD ioctlFlags)
{
  wlct_os_err_t res = WLCT_OS_ERROR_GEN_FAILURE;
  wlct_cdev_ioctl_hdr_t *ioctl_hdr = NULL;

  WLCT_ASSERT(IsOpened() == true);
  WLCT_ASSERT(dataBuf != NULL);
  WLCT_ASSERT(dataBufLen != 0);

  ioctl_hdr = (wlct_cdev_ioctl_hdr_t *)malloc((sizeof *ioctl_hdr) + dataBufLen);
  if (NULL == ioctl_hdr)
  {
    LOG_MESSAGE_ERROR("Allocation failed (data of %u bytes)\n", dataBufLen);
    res = WLCT_OS_ERROR_NOT_ENOUGH_MEMORY;
    goto Exit;
  }

  ioctl_hdr->magic = WLCT_IOCTL_MAGIC;
  ioctl_hdr->len = dataBufLen;
  ioctl_hdr->flags = ioctlFlags;

  if (ioctlFlags & WLCT_IOCTL_FLAG_SET)
    memcpy(ioctl_hdr + 1, dataBuf, dataBufLen);

  res = ioctl(fd, 0, ioctl_hdr);
  if (0 != res)
  {
    res = errno;
    LOG_MESSAGE_ERROR("ioctl failed with error code %d", res);
    goto Exit;
  }

  if (ioctlFlags & WLCT_IOCTL_FLAG_GET)
    memcpy(dataBuf, ioctl_hdr + 1, dataBufLen);

Exit:
  if (NULL != ioctl_hdr)
    free(ioctl_hdr);

  return res;
}
