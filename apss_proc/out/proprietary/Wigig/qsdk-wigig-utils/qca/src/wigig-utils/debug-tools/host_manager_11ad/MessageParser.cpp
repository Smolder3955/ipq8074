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

#include "MessageParser.h"
#include <sstream>

using namespace std;

// *************************************************************************************************

MessageParser::MessageParser(const string& message)
    :m_message(message)
{
    char delimeter = '|'; //The new separator in a message is: "|", the old one is " ", keep the old one for backward compatibility
    if (message.find_first_of(delimeter) == string::npos)
    { //The new separator is *not* in the message, that means that the message is in the old format
        delimeter = ' ';
    }
    m_splitMessage = m_SplitMessageByDelim(m_message, delimeter);
}

// *************************************************************************************************
vector<string> MessageParser::m_SplitMessageByDelim(const string &message, char delim)
{
    vector<string> splitMessage;
    stringstream sstream(message);
    string word;
    while (getline(sstream, word, delim))
    {
        //if (word.empty())
        //{ //don't push whitespace
        //    continue;
        //}
        splitMessage.push_back(word);
    }
    return splitMessage;
}

// *************************************************************************************************

string MessageParser::GetCommandFromMessage()
{
    return string(m_splitMessage.front());

}


// *************************************************************************************************

vector<string> MessageParser::GetArgsFromMessage()
{
    vector<string> temp = m_splitMessage;
    temp.erase(temp.begin());
    return temp;
}

// *************************************************************************************************

unsigned int MessageParser::GetNumberOfArgs()
{
    return static_cast<unsigned>(m_splitMessage.size() - 1); //(-1) because the function name is in also in the message
}
