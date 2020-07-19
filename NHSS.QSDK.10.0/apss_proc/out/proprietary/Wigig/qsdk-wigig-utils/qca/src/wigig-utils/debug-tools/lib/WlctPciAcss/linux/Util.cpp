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

#include "Util.h"

#ifdef NO_GCC_ATOMIC
#error "Platform doesn't support GCC atomic built-ins"
#endif

#define InterlockedExchange __sync_lock_test_and_set

BOOL Util::timedResourceInterLockExchange ( LONG* var, LONG flag_value, int timeout, bool keepWaiting )
{
  // perform atomic change value, to acquire resource!
  // if VAR != FLAG_VALUE, then set VAR to FLAG_VALUE.
  // loop until timeout (millisecond). return success in achieving resource
  // Assumes that FLAG_VALUE should be different then current value stored in VAR
  // turn keepWaiting on to wait infinite

  LONG original_value;

  int i = 0;
  do
  {
    original_value = InterlockedExchange(var, flag_value);
    if (original_value != flag_value)
    {
      return true;
    }
    if (timeout > 0 )
    {
      sleep_ms(1);
    }
    i++;
  } while ((i < timeout) || (keepWaiting));

  return false;
}

bool Util::ICHDisableEnable()
{
  return false;
}

DWORD Util::ReadRegistrySettings(LPCTSTR lpszRegistryPath, LPCTSTR valName)
{
  //do something with params
  (void)lpszRegistryPath;
  (void)valName;

  return -1;
}

DWORD Util::WriteRegistrySettings(LPCTSTR lpszRegistryPath, LPCTSTR valName, DWORD val)
{
  //do something with params
  (void)lpszRegistryPath;
  (void)valName;
  (void)val;

  return -1;
}
