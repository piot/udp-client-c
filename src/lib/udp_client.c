/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#if defined TORNADO_OS_WINDOWS
#include <WinSock2.h>
typedef USHORT in_port_t;
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#include <ws2tcpip.h>
#endif
#define UDP_CLIENT_SIZE_CAST(a) (int) a
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#define UDP_CLIENT_SIZE_CAST(a) a
#endif

#include <clog/clog.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <udp-client/udp_client.h>

#include <tiny-libc/tiny_libc.h>

static int socket_non_blocking(UDP_CLIENT_SOCKET_HANDLE handle, int non_blocking)
{
#if defined TORNADO_OS_WINDOWS
    u_long mode = non_blocking;
    int result = ioctlsocket(handle, FIONBIO, &mode);
    if (result != NO_ERROR) {
        return result; // udp_client_error(result, "ioctlsocket failed with error");
    }
    return 0;
#else
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(handle, F_SETFL, flags);
#endif
}

static int udpClientBind(UDP_CLIENT_SOCKET_HANDLE handle, in_port_t port)
{
    struct sockaddr_in servaddr;

    tc_mem_clear_type(&servaddr);

    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    int result;
    if ((result = bind(handle, (const struct sockaddr*) &servaddr, sizeof(servaddr))) < 0) {
        CLOG_WARN("could not bind to port %d", port)
        return result;
    }

    return 0;
}

static UDP_CLIENT_SOCKET_HANDLE create(void)
{
    UDP_CLIENT_SOCKET_HANDLE handle = socket(PF_INET, SOCK_DGRAM, 0);
    socket_non_blocking(handle, 1);
    return handle;
}

static void setPeerAddress(UdpClientSocket* self, const char* name, uint16_t port)
{
    struct sockaddr_in* peer = &self->peer_address;

    struct addrinfo hints;
    tc_mem_clear_type(&hints);
    hints.ai_family = AF_UNSPEC; // AF_INET
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* result;

    int s = getaddrinfo(name, 0, &hints, &result);
    if (s < 0) {
        CLOG_WARN("set_peer_address Error!%d", s)
        return;
    }

    const struct addrinfo* first = result;
    tc_memset_octets(peer, 0, sizeof(*peer));
    struct sockaddr_in* in_addr = (struct sockaddr_in*) first->ai_addr;
    tc_memcpy_octets(peer, in_addr, first->ai_addrlen);
    peer->sin_port = htons(port);
    freeaddrinfo(result);
}

/// Initializes the socket API. Only call once at startup.
/// Only needed on certain platforms.
/// @return negative on error
int udpClientStartup(void)
{
#if defined TORNADO_OS_WINDOWS
    WSADATA wsaData;
    int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (err != 0) {
        if (wsaData.wHighVersion != 2 || wsaData.wVersion != 2) {
            return -3;
        }
        return -1;
    }
    return 0;
#else
    return 0;
#endif
}

/// Initialize the UDP client.
/// Make sure that udpClientStartup() is called once at startup before calling this function.
/// @param self udp client
/// @param name the name of the host to send to
/// @param port the UDP port to send to
/// @return negative on error
int udpClientInit(UdpClientSocket* self, const char* name, uint16_t port)
{
    self->handle = create();

    int result;
    if ((result = udpClientBind(self->handle, 0)) < 0) {
        return result;
    }
    setPeerAddress(self, name, port);
    return 0;
}

/// Sends an UDP packet with the specified payload.
/// Note: On MacOS the maximum size is 9216 octets.
/// @param self client socket
/// @param data the data to be sent
/// @param size the octet count of the data
/// @return negative number is an error code
int udpClientSend(UdpClientSocket* self, const uint8_t* data, size_t size)
{
    if (size > UDP_CLIENT_MAX_OCTET_SIZE) {
        CLOG_ERROR("You wanted to send %zu octets, but the recommended maximum size is %zu", size,
                   UDP_CLIENT_MAX_OCTET_SIZE)
        // return -2;
    }
    if (size == 0) {
        CLOG_SOFT_ERROR("udpClientSend: you can not send zero length packets")
        return -4;
    }

    ssize_t number_of_octets_sent = sendto(self->handle, (const char*) data, UDP_CLIENT_SIZE_CAST(size), 0,
                                           (struct sockaddr*) &self->peer_address, sizeof(self->peer_address));

    if (number_of_octets_sent < 0) {
        CLOG_WARN("Error send! errno:%d return: %zd", UDP_CLIENT_GET_ERROR, number_of_octets_sent)
        return -1;
    }

    return ((size_t) number_of_octets_sent == size);
}

/// Try to receive an UDP packet
/// If not available, it returns zero.
/// @param self client socket
/// @param data target buffer for packet payload
/// @param size maximum size of data buffer
/// @return returns zero if it would block, or if no packet is available, negative numbers for error. positive numbers
/// is the number of octets in the payload.
ssize_t udpClientReceive(UdpClientSocket* self, uint8_t* data, size_t size)
{
    struct sockaddr_in from_who;

    if (size != UDP_CLIENT_MAX_OCTET_SIZE) {
        CLOG_SOFT_ERROR(
            "udpClientReceive: packet buffer target should be the recommended size: %zu but encountered %zu",
            UDP_CLIENT_MAX_OCTET_SIZE, size)
        return -2;
    }

    socklen_t addr_size = sizeof(from_who);
    ssize_t number_of_octets = recvfrom(self->handle, (char*) data, UDP_CLIENT_SIZE_CAST(size), 0, (struct sockaddr*) &from_who,
                                        &addr_size);
    if (number_of_octets == -1) {
        int last_err = UDP_CLIENT_GET_ERROR;
        if (last_err == UDP_CLIENT_ERROR_AGAIN || last_err == UDP_CLIENT_ERROR_WOULDBLOCK) {
            return 0;
        } else {
            CLOG_WARN("udpClientReceive: error %d", last_err)
            return 0;
        }
    }

    return number_of_octets;
}
