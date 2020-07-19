/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <cstdio>
#include <exception>
#include <iostream>

#include "CommandLineArgs.h"
#include "TcpClient.h"
#include "CommandHandler.h"
#include "LogHandler.h"

int main(int argc, char* argv[])
{
    CommandHandlerResult res;
    try
    {
        g_ApplicationName = argv[0];
        CommandLineArgs commandLineArgs(argv, argc);
        CommandHandler ch;
        res = ch.HandleCommand(commandLineArgs);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << "Unexpected error occurred:\n" << e.what() << std::endl;
        return CH_ERROR;
    }

    return res;
}

