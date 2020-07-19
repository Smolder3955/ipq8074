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

#ifndef _PARAMETER_PARSER_H
#define _PARAMETER_PARSER_H

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <map>
#include <list>
#include <algorithm>

using namespace std;

class ParameterParser
{
    map<string, char*> Parameters;
    map<string, bool> Flags;
    list<string> flags;
    list<string> params;

public:
    ParameterParser();
    ~ParameterParser();

    int processCommandArgs( int _argc, char **_argv );

    char *getValue(string _option);
    bool  getFlag(string _option);

    void printUsage();

    void printHelp();

    void setVerbose();

    void addFlag(string flag);
    void addParam(string param);

private:
    int argc;
    char **argv;

    bool verbose;

    void printVerbose( const char *INFO );
    void printVerbose( char *INFO );
    void printVerbose( char ch );
    void printVerbose( );

    bool IsFlag(string flag);
    bool IsParam(string param);

    bool ParamFound(string param);

    bool FlagFound(string flag);
};

#endif /* ! _PARAMETER_PARSER_H */
