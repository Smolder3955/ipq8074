/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef SHELL_COMMAND_EXECUTOR_H_
#define SHELL_COMMAND_EXECUTOR_H_

#include <vector>
#include <string>

// Executes a shell command

class ShellCommandExecutor final
{
public:

    explicit ShellCommandExecutor(const char* szCmd);

    const std::string& Command() const { return m_Command; }
    int ExitStatus() const { return m_ExitStatus; }
    const std::vector<std::string>& Output() const { return m_Output; }

    std::string ToString() const;

    // Non-copyable
    ShellCommandExecutor(const ShellCommandExecutor&) = delete;
    ShellCommandExecutor& operator=(const ShellCommandExecutor&) = delete;

private:

    int m_ExitStatus;                   // Shell exit status of the last executed command
    std::string m_Command;              // Last executed command
    std::vector<std::string> m_Output;  // Output of the last command (if any) or invocation error message

};

std::ostream& operator<<(std::ostream& os, const ShellCommandExecutor& shellCmdExecutor);

#endif
