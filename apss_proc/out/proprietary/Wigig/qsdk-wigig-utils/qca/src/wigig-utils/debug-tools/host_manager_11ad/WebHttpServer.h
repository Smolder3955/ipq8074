/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _WEBHTTPSERVER_H_
#define _WEBHTTPSERVER_H_

#include "CommandsExecutor.h"

using namespace std;

class Host;
class HttpServer;


class WebHttpServer
{
public:

    /*
    * Web Http server constructor gets the port to start in.
    */
    WebHttpServer(unsigned int httpPort);

    /*
    * Start the web HTTP server at the given port (given in the constructor) and configures its routes
    */
    void StartServer();

    /*
    * Stop the web Http server
    */
    void Stop();

private:
    unsigned int m_port; //http port
    //internal web server implementation Simple Web Server
    shared_ptr<HttpServer> m_pServer;
    //real command processor that works with Device Manager
    shared_ptr<CommandsExecutor> m_pCommandsExecutor;

    //configures handling of http requests
    void ConfigureRoutes();

    string GetFileName(string requestPath);
    string GetFileContentType(string fileName);

    string GetHostDataJson();
    void PerformJsonHostCommands(const string& json);
};


#endif // !_WEBHTTPSERVER_H_
