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

#include <string>
#include <map>
#include <vector>
#include <sstream>

#define WSTR_ARG(s) ToWString(s).c_str()
#define TSTR_ARG(s) ToTString(s).c_str()

#if defined(UNICODE) || defined(_UNICODE)
typedef std::wstring         tstring;
typedef std::wstringstream   tstringstream;
typedef std::wostringstream  tostringstream;
#else
typedef std::string          tstring;
typedef std::stringstream    tstringstream;
typedef std::ostringstream   tostringstream;
#endif

typedef std::map<tstring, tstring> TStringMap;
typedef std::vector<std::string> TStringList;

inline std::wstring ToWString(const char *str)
{
  std::wostringstream s;
  s << str;
  return s.str();
}

inline std::wstring ToWString(const wchar_t *str)
{
  return str;
}

inline tstring ToTString(const char *str)
{
  tostringstream s;
  s << str;
  return s.str();
}

// printf format specifiers
//
//| Format Specifier | Win printf | Win wprintf | Lin printf | Lin wprintf |
//|------------------|------------| ----------- | ---------- | ----------- |
//|      %s          | char*      | wchar_t*    | char*      | char*       |
//|      %ls         | wchar_t*   | wchar_t*    | wchar_t*   | wchar_t*    |
//|      %hs         | char*      | char*       |       NOT SUPPORTED      |
//|      $S          | wchar_t*   | char*       | wchar_t*   | wchar_t*    |
//|------------------|------------| ----------- | ---------- | ----------- |
