/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

/// Maximum size is 1200, since the general recommendations are somewhere between 1200 and 1400 octets.
/// (https://www.ietf.org/id/draft-ietf-dnsop-avoid-fragmentation-06.html#section-3.3).
/// Steam Networking has a 1200 octet packet size limit (https://partner.steamgames.com/doc/api/ISteamNetworking)
static const size_t UDP_CLIENT_MAX_OCTET_SIZE = 1200;

#if defined TORNADO_OS_WINDOWS
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#endif

#include <stdint.h>


#if defined TORNADO_OS_WINDOWS
#define UDP_CLIENT_SOCKET_HANDLE SOCKET
#define UDP_CLIENT_SOCKET_CLOSE closesocket
#define UDP_CLIENT_ERROR_INPROGRESS WSAEINPROGRESS
#define UDP_CLIENT_ERROR_WOULDBLOCK WSAEWOULDBLOCK
#define UDP_CLIENT_ERROR_AGAIN WSAEINPROGRESS
#define UDP_CLIENT_ERROR_NOT_CONNECTED WSAENOTCONN
#define UDP_CLIENT_GET_ERROR WSAGetLastError()
#else
#define UDP_CLIENT_SHUTDOWN_READ_WRITE SHUT_RDWR
#define UDP_CLIENT_ERROR_INPROGRESS EINPROGRESS
#define UDP_CLIENT_ERROR_WOULDBLOCK EINPROGRESS
#define UDP_CLIENT_ERROR_AGAIN EAGAIN
#define UDP_CLIENT_SOCKET_HANDLE int
#define UDP_CLIENT_INVALID_SOCKET_HANDLE (-1)
#include <unistd.h>
#define UDP_CLIENT_SOCKET_CLOSE close
#define UDP_CLIENT_GET_ERROR errno
#endif



typedef struct UdpClientSocket {
    UDP_CLIENT_SOCKET_HANDLE handle;
    struct sockaddr_in peer_address;
} UdpClientSocket;

int udpClientStartup(void);

int udpClientInit(UdpClientSocket* self, const char* name, uint16_t port);
int udpClientSend(UdpClientSocket* self, const uint8_t* data, size_t size);
ssize_t udpClientReceive(UdpClientSocket* self, uint8_t* data, size_t size);

#endif
