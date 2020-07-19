/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _JSONCOMMANDSTCPSERVER_H_
#define _JSONCOMMANDSTCPSERVER_H_

#include <memory>
#include <string>

#include "HostDefinitions.h"

namespace TcpNetworkInterfaces {
    class NetworkInterface;
}

class Host;

/*
* Json Commands TCP Server is a synchronous server that handles each client separately.
* When a client sends a new json command to the Json Commands TCP Server, the host processes it and sends back to the client an answer.
*/
class JsonCommandsTcpServer
{
public:

    /*
    * Json Commands TCP server constructor gets the port to start in. It also initializes new Socket object.
    */
    JsonCommandsTcpServer(unsigned int jsonCommandsTcpPort, Host& host);

    /*
    * Start the json commands TCP server at the given port (given in the constructor).
    * For each new client that is connecting to the server it opens a new thread and
    * continue to listen on the port for more clients.
    */
    void Start();

    /*
    * Stop the json commands TCP server by doing some clean ups for the sockets.
    */
    void Stop();

private:
    unsigned int m_port; //The port in which the json commands TCP server is working on
    std::unique_ptr<TcpNetworkInterfaces::NetworkInterface> m_pSocket; //an object that holds the connection with each client
    Host& m_host; // reference to the host object (that is passed to jsonCommandsHandler each time a new TCP connection is created)

    void ServerThread(std::unique_ptr<TcpNetworkInterfaces::NetworkInterface> client);
    ConnectionStatus Reply(TcpNetworkInterfaces::NetworkInterface *client, std::string &responseMessage);

    bool m_running;
};
#endif // !_JSONCOMMANDSTCPSERVER_H_
