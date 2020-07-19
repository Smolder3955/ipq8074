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

#ifndef _SOCKET_H_
#define _SOCKET_H_

#ifdef _WINDOWS
    #pragma comment(lib, "Ws2_32.lib")
#endif

#include <string>
#include <vector>
#include <memory>

#ifdef _WINDOWS
    #include <winsock2.h>
    #include <Ws2tcpip.h>/* Added to support IPv6*/
#elif __linux
    #include <unistd.h>
    #include <netdb.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/wait.h>
#else
    #include <sys/socket.h>
    #include <net/route.h>
    #include <net/if.h>
    #include <arpa/inet.h>
#endif

namespace TcpNetworkInterfaces
{
    class NetworkInterface
    {
    public:

        explicit NetworkInterface(const char* szName);
        NetworkInterface(const char* szName, int sockfd);
        ~NetworkInterface();

        // Initialization of the network (necessary for Windows platforms)
        static bool InitializeNetwork();
        static void FinalizeNetwork();

        // Establish Connection
        void Bind(int portNumber);
        void Listen(int backlog = 5);
        std::unique_ptr<NetworkInterface> Accept();

        // Send and Receive
        bool SendString(const std::string& text);
        bool SendString(const char* szText);
        bool SendBuffer(const char* pBuf, size_t bufSize);

        bool Receive(std::string& message);
        const char* BinaryReceive(int size, int flags = 0);

        // Terminate Connection
        void Close();

        // Addresses
        const char* GetPeerName() const;
        void UpdatePeerNameInternal();

        // Delete copy c'tor and assignment operator because of the std::vector<char> m_commandBuffer (so it won't be released twice)
        NetworkInterface(const NetworkInterface&) = delete;
        NetworkInterface& operator=(const NetworkInterface&) = delete;

    private:
        enum { MAX_COMMAND_BUFFER_SIZE = 4096 };

        int m_fileDescriptor;
        struct sockaddr_in6 m_localAddress;
        char* m_buffer;
        size_t m_bufferSize;
        std::vector<char> m_commandBuffer;
        std::string m_peerName;
        const std::string m_channelName;
    };

}

#endif // !_NETWORK_INTERFACE_H_
