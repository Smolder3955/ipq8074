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

#ifndef _ARGUMENTSPARSER_H_
#define _ARGUMENTSPARSER_H_

#include <string>
#include <vector>

// *************************************************************************************************
/*
 * Class for parsing the arguments given in the command line
 */
class ArgumentsParser
{
public:
    // return value - true for continue running host manager, false for exit
    bool ParseAndHandleArguments(int argc, char* argv[]);

    const std::string& GetCustomConfigFile() const { return m_customConfigFile; }

private:
    std::vector<std::string> m_arguments; //Vector that holds each one of the arguments that was given in the command line
    bool DoesArgumentExist(const std::string& option);
    bool GetArgumentValue(const std::string& option, unsigned& val); // returns true iff the argument exists in the arguments list and its value was extracted correctly
    bool GetArgumentValueStr(const std::string& option, std::string& val); // returns true iff the argument exists in the arguments list and its value was extracted correctly

    std::string m_customConfigFile;
};

#endif // !_ARGUMENTSPARSER_H_
