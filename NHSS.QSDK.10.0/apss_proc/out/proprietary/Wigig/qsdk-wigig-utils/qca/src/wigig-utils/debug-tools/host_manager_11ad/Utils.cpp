/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
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

#include "Utils.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cstdlib>
#include <algorithm>
#include <iterator>
#include "DebugLogger.h"

#if _WINDOWS
#define localtime_r(_Time, _Tm) localtime_s(_Tm, _Time)
#endif

using namespace std;

const unsigned int Utils::REGISTER_DEFAULT_VALUE = 0xDEADDEAD;

const string Utils::PCI = "PCI";
const string Utils::RTL = "RTL";
const string Utils::JTAG = "JTAG";
const string Utils::SERIAL = "SERIAL";
const string Utils::DUMMY = "DUMMY";
const string Utils::SPI = "SPI";


// *************************************************************************************************
vector<string> Utils::Split(string str, char delimiter)
{
    vector<string> splitStr;
    size_t nextSpacePosition = str.find_first_of(delimiter);
    while (string::npos != nextSpacePosition)
    {
        splitStr.push_back(str.substr(0, nextSpacePosition));
        str = str.substr(nextSpacePosition + 1);
        nextSpacePosition = str.find_first_of(delimiter);
    }

    if ("" != str)
    {
        splitStr.push_back(str);
    }
    return splitStr;
    /*
      vector<string> splitMessage;
      stringstream sstream(message);
      string word;
      while (getline(sstream, word, delim))
      {
      if (word.empty())
      { //don't push whitespace
      continue;
      }
      splitMessage.push_back(word);
      }
      return splitMessage;
    */
}

// *************************************************************************************************
string Utils::Concatenate(const vector<string>& vec, const char* delimiter)
{
    if (vec.empty())
    {
        return "";
    }
    if (vec.size() == 1U)
    {
        return vec.front();
    }

    stringstream ss;
    std::copy(vec.cbegin(), std::prev(vec.cend()), ostream_iterator<string>(ss, delimiter));
    ss << vec.back();
    return ss.str();
}

std::string Utils::GetTimeString(const TimeStamp &ts)
{
    int tm_hourTs = ts.m_localTime.tm_hour;
    std::string ampm = tm_hourTs>=12 ? "PM" : "AM" ;
    if (tm_hourTs > 12)
    {
        tm_hourTs -= 12;
    }
    stringstream timeStampBuilder;

    timeStampBuilder << "[" << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_mon + 1 << "/" <<
        std::setfill('0') << std::setw(2) << ts.m_localTime.tm_mday << "/" <<
        std::setfill('0') << std::setw(2) << ts.m_localTime.tm_year + 1900 << " " <<
        std::setfill('0') << std::setw(2) << tm_hourTs << ":" <<
        std::setfill('0') << std::setw(2) << ts.m_localTime.tm_min << ":" <<
        std::setfill('0') << std::setw(2) << ts.m_localTime.tm_sec << "." <<
        std::setfill('0') << std::setw(3) << ts.m_milliseconds << " " << ampm << "]";
    return timeStampBuilder.str();
}

// *************************************************************************************************
TimeStamp Utils::GetCurrentLocalTime()
{
    TimeStamp ts;
    chrono::system_clock::time_point nowTimePoint = chrono::system_clock::now(); // get current time

    // convert epoch time to struct with year, month, day, hour, minute, second fields
    time_t now = chrono::system_clock::to_time_t(nowTimePoint);
    localtime_r(&now, &ts.m_localTime);

    // get milliseconds field
    const chrono::duration<double> tse = nowTimePoint.time_since_epoch();
    ts.m_milliseconds = chrono::duration_cast<std::chrono::milliseconds>(tse).count() % 1000;
    return ts;
}


// *************************************************************************************************
string Utils::GetCurrentLocalTimeString()
{

    TimeStamp ts = Utils::GetCurrentLocalTime();

    ostringstream currentTime;
    currentTime << (1900 + ts.m_localTime.tm_year) << '-'
                << std::setfill('0') << std::setw(2) << (ts.m_localTime.tm_mon + 1) << '-'
                << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_mday << ' '
                << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_hour << ':'
                << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_min << ':'
                << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_sec << '.'
                << std::setfill('0') << std::setw(3) << ts.m_milliseconds;

    string currentTimeStr(currentTime.str());
    return currentTimeStr;
}
// *************************************************************************************************

string Utils::GetCurrentDotNetDateTimeString()
{
    TimeStamp ts = Utils::GetCurrentLocalTime();
    time_t now = time(0); // UTC
    time_t diff;
    struct tm *ptmgm = gmtime(&now); // further convert to GMT presuming now in local
    if (!ptmgm)
    {   // shouldn't happen
        return "";
    }
    time_t gmnow = mktime(ptmgm);
    diff = gmnow - now;
    if (ptmgm->tm_isdst > 0)
    {
        diff = diff - 60 * 60;
    }
    diff /= 3600;
    std::string timezoneSeperator = diff >= 0 ? "+" : "-";
    ostringstream timezoneStream;
    auto absDiff = std::labs(static_cast<long>(diff));
    if(absDiff < 10)
    {
        timezoneStream << "0" << absDiff << ":00";
    }
    else
    {
        timezoneStream << absDiff << ":00";
    }

        ostringstream currentTime;
        currentTime << (1900 + ts.m_localTime.tm_year) << '-'
                    << std::setfill('0') << std::setw(2) << (ts.m_localTime.tm_mon + 1) << '-'
                    << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_mday << 'T'
                    << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_hour << ':'
                    << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_min << ':'
                    << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_sec << '.'
                    << std::setfill('0') << std::setw(3) << ts.m_milliseconds << timezoneSeperator
                    << timezoneStream.str();

        string currentTimeStr(currentTime.str());
        return currentTimeStr;
}

// *************************************************************************************************
string Utils::GetCurrentLocalTimeXml(const TimeStamp &ts)
{
    ostringstream timeStampBuilder;

    timeStampBuilder << "<Log_Content>"
        << "<Sample_Time>"
        << "<Hour>" << ts.m_localTime.tm_hour << "</Hour>"
        << "<Minute>" << ts.m_localTime.tm_min << "</Minute>"
        << "<Second>" << ts.m_localTime.tm_sec << "</Second>"
        << "<Milliseconds>" << ts.m_milliseconds << "</Milliseconds>"
        << "<Day>" << ts.m_localTime.tm_mday << "</Day>"
        << "<Month>" << ts.m_localTime.tm_mon + 1 << "</Month>"
        << "<Year>" << ts.m_localTime.tm_year + 1900 << "</Year>"
        << "</Sample_Time>";

    return timeStampBuilder.str();
}

// *************************************************************************************************
string Utils::GetCurrentLocalTimeForFileName()
{

    TimeStamp ts = Utils::GetCurrentLocalTime();

    ostringstream currentTime;
    currentTime << (1900 + ts.m_localTime.tm_year)
        << std::setfill('0') << std::setw(2) << (ts.m_localTime.tm_mon + 1)
        << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_mday << '-'
        << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_hour
        << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_min
        << std::setfill('0') << std::setw(2) << ts.m_localTime.tm_sec << '-'
        << std::setfill('0') << std::setw(3) << ts.m_milliseconds;

    string currentTimeStr(currentTime.str());
    return currentTimeStr;
}

// *************************************************************************************************
bool Utils::ConvertHexStringToDword(string str, DWORD& word)
{
    if (str.find_first_of("0x") != 0) //The parameter is a hex string (assuming that a string starting with 0x must be hex)
    {
        return false;
    }
    istringstream s(str);
    s >> hex >> word;
    return true;
}

// *************************************************************************************************
bool Utils::ConvertHexStringToDwordVector(string str, char delimiter, vector<DWORD>& values)
{
    vector<string> strValues = Utils::Split(str, delimiter);
    values.reserve(strValues.size());
    for (auto& strValue : strValues)
    {
        DWORD word = 0;
        if (Utils::ConvertHexStringToDword(strValue, word))
        {
            values.push_back(word);
        }
        else
        {
            return false;
        }
    }

    return true;
}

// *************************************************************************************************
bool Utils::ConvertDecimalStringToUnsignedInt(string str, unsigned int& ui)
{
    unsigned long l;
    try
    {
        l = strtoul(str.c_str(), nullptr, 10); // 10 for decimal base
    }
    catch (...)
    {
        return false;
    }

    ui = l;
    return true;
}


bool Utils::ConvertStringToBool(string str, bool& boolVal)
{
    string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);

    if (lowerStr.compare("false") == 0)
    {
        boolVal = false;
        return true;
    }
    else if (lowerStr.compare("true") == 0)
    {
        boolVal = true;
        return true;
    }
    else
    {
        return false;
    }
}

// Encode given binary array as Base64 string
string Utils::Base64Encode(const vector<unsigned char>& binaryData)
{
    return Base64Encode(binaryData.data(), binaryData.size());
}

string Utils::Base64Encode(const unsigned char binaryData[], size_t length)
{
    static const std::string base64Vocabulary = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    if (length == 0U)
    {
        return result;
    }

    // reserve room for the result (next higher multiple of 3, div by 3, mult by 4)
    unsigned mod3 = length % 3;
    result.reserve((length + (mod3 ? 3 - mod3 : 0)) * 4 / 3);

    int val1 = 0, val2 = -6;
    for (size_t i = 0U; i < length; ++i)
    {
        val1 = (val1 << 8) + static_cast<int>(binaryData[i]);
        val2 += 8;
        while (val2 >= 0)
        {
            result.push_back(base64Vocabulary[(val1 >> val2) & 0x3F]);
            val2 -= 6;
        }
    }

    if (val2 > -6)
    {
        result.push_back(base64Vocabulary[((val1 << 8) >> (val2 + 8)) & 0x3F]);
    }

    while (result.size() % 4)
    {
        result.push_back('=');
    }

    return result;
}

// Decode the string given in Base64 format
bool Utils::Base64Decode(const string& base64Data, vector<unsigned char>& binaryData)
{
    static const unsigned char lookup[] =
    {
        62,  255, 62,  255, 63,  52,  53, 54, 55, 56, 57, 58, 59, 60, 61, 255,
        255, 0,   255, 255, 255, 255, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
        10,  11,  12,  13,  14,  15,  16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
        255, 255, 255, 255, 63,  255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
        36,  37,  38,  39,  40,  41,  42, 43, 44, 45, 46, 47, 48, 49, 50, 51
    };

    binaryData.clear();
    if (base64Data.empty())
    {
        return false;
    }
    binaryData.reserve(base64Data.size() * 3 / 4);

    int val1 = 0, val2 = -8;
    for (const auto c : base64Data)
    {
        if (c < '+' || c > 'z')
        {
            break;
        }

        int mappedVal = static_cast<int>(lookup[c - '+']);
        if (mappedVal >= 64)
        {
            break;
        }

        val1 = (val1 << 6) + mappedVal;
        val2 += 6;

        if (val2 >= 0)
        {
            binaryData.push_back(char((val1 >> val2) & 0xFF));
            val2 -= 8;
        }
    }

    return true;
}
