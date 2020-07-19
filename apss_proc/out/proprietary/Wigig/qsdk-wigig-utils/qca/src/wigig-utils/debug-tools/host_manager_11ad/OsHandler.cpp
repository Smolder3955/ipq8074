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

#ifdef _WINDOWS
#include <windows.h>   // Sleep()
#elif __OS3__
#include <sys/signal.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

#include "OsHandler.h"
#include "DebugLogger.h"
#include "Host.h"

using namespace std; //TODO - maybe to remove

#ifndef _WINDOWS

void sig_quit_handler(int signum)
{
    if (signum == SIGPIPE) // this signal is sent to host manager when DmTools tries to reconnect after DmTools was restarted
    {
        //printf("Connection lost\n");
        return;
    }

    LOG_INFO << "Exiting host_manager_11ad as per user request (got signal " << signum << ")" << std::endl;
    Host::GetHost().StopHost();
    exit(0);
}
#endif

// *************************************************************************************************

void OsHandler::HandleOsSignals()
{
#ifndef _WINDOWS
    //LOG_INFO << "Handle linux SIQQUIT signal" << endl;
    signal(SIGQUIT, sig_quit_handler);
    signal(SIGINT, sig_quit_handler);
    signal(SIGPIPE, sig_quit_handler);
#endif

}

// *************************************************************************************************

void OsHandler::OsSleep(int sleepTimeMsec)
{
#ifdef _WINDOWS
    Sleep(sleepTimeMsec);
#else
    usleep(sleepTimeMsec * 1000);   // uSec
#endif
}

//void OsHandler::OsError(const char* error_message, ...)
//{
//    va_list argptr;
//    va_start(argptr, error_message);
//
//#ifdef _WINDOWS
//    vfprintf(stderr, error_message, argptr);
//
//    exit(0);
//#else
//    vfprintf(stderr, error_message, argptr);
//
//    exit(0);
//#endif
//
//    va_end(argptr);
//}

