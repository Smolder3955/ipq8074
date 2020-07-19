/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "ShellCommandExecutor.h"
#include "DebugLogger.h"

#include <string>
#include <sstream>
#include <memory>
#include <cstddef>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef  _WINDOWS
#include <sys/wait.h>
#endif // ! _WINDOWS

#ifdef _WINDOWS
#define popen _popen
#define pclose _pclose
#endif



// *************************************************************************************************

ShellCommandExecutor::ShellCommandExecutor(const char* szCmd)
    : m_ExitStatus(0)
{
    if (szCmd == nullptr)
    {
        LOG_ERROR << "No command provided" << std::endl;
        m_ExitStatus = -1;
        m_Command = "<null>";
        m_Output.push_back("No command provided");
        return;
    }

    // Redirect stderr to stdout to get all printed output
    std::stringstream ss;
    ss << szCmd << " 2>&1";
    m_Command = ss.str();
    LOG_DEBUG << "Executing shell command: " << m_Command << std::endl;

    FILE* pCommandOutputStream = popen(m_Command.c_str(), "r");
    if (pCommandOutputStream == nullptr)
    {
        std::stringstream err;
        err << "Failed to run shell command: " << szCmd << " Error: " << strerror(errno) << std::endl;
        m_ExitStatus = -1;
        m_Output.push_back(err.str());
        return;
    }

    // Temporary buffer for command output
    static const size_t BUF_SIZE = 256;
    std::unique_ptr<char[]> spBuf(new char[BUF_SIZE]);

    while (fgets(spBuf.get(), BUF_SIZE, pCommandOutputStream) != nullptr)
    {
        // Overwrite trailing newline character
        size_t lineLength = strlen(spBuf.get());
        if (0 == lineLength) continue;   // Should not happen
        if ('\n' == spBuf[lineLength-1])
        {
            spBuf[lineLength-1] = '\0';
        }

        m_Output.push_back(spBuf.get());
    }

    int pcloseStatus = pclose(pCommandOutputStream);
    LOG_VERBOSE << "pclose status: " << Hex<>(pcloseStatus) << std::endl;
#ifdef _WINDOWS
    m_ExitStatus = pcloseStatus;
#else
    if (WIFEXITED(pcloseStatus))
    {
        // Get a low-order 8 bits of the exit status value from the child process.
        m_ExitStatus = WEXITSTATUS(pcloseStatus);
    }
    else
    {
        LOG_ERROR << "Cannot determine the exit status of a shell process" << std::endl;
        m_ExitStatus = -1;
    }
#endif // _WINDOWS

    LOG_DEBUG << "Command exit status: " << Hex<>(m_ExitStatus)
              << " (" << m_ExitStatus << ")" << std::endl;
}

// *************************************************************************************************

std::string ShellCommandExecutor::ToString() const
{
    std::stringstream ss;
    ss << (*this);
    return ss.str();
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const ShellCommandExecutor& shellCmdExecutor)
{
    os << "Command: '" << shellCmdExecutor.Command() << "'"
       << " Exit Code: " << shellCmdExecutor.ExitStatus()
       << " Shell Output: [ ";

    for (const std::string& outLine: shellCmdExecutor.Output())
    {
        os << outLine << "\\n";
    }

    return os << ']' << std::endl;
}

