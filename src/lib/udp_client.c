/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#if TORNADO_OS_WINDOWS
#include <WinSock2.h>
typedef USHORT in_port_t;
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#include <ws2tcpip.h>
#endif

#else
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <clog/clog.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <udp-client/udp_client.h>

static int socket_non_blocking(int handle, int non_blocking)
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

static int udpClientBind(int handle, in_port_t port)
{
    struct sockaddr_in servaddr;

    tc_mem_clear_type(&servaddr);

    servaddr.sin_family = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    int result;
    if ((result = bind(handle, (const struct sockaddr*)&servaddr, sizeof(servaddr))) < 0) {
        CLOG_WARN("could not bind to port %d", port);
        return result;
    }

    return 0;
}

static int create(void)
{
    int handle = socket(PF_INET, SOCK_DGRAM, 0);
    socket_non_blocking(handle, 1);
    return handle;
}

static void setPeerAddress(UdpClientSocket* self, const char* name, uint16_t port)
{
    struct sockaddr_in* peer = &self->peer_address;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // AF_INET
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo* result;

    int s = getaddrinfo(name, 0, &hints, &result);
    if (s < 0) {
        CLOG_WARN("set_peer_address Error!%d", s)
        return;
    }

    const struct addrinfo* first = result;
    memset(peer, 0, sizeof(*peer));
    struct sockaddr_in* in_addr = (struct sockaddr_in*)first->ai_addr;
    memcpy(peer, in_addr, first->ai_addrlen);
    peer->sin_port = htons(port);
    freeaddrinfo(result);
}

/// Initializes the socket API. Only call once at startup.
/// Only needed on certain platforms.
/// @return
int udpClientStartup(void)
{
#if TORNADO_OS_WINDOWS
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
/// @param self
/// @param name the name of the host to send to
/// @param port the UDP port to send to
/// @return
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

#define UDP_CLIENT_MAX_OCTET_SIZE (size_t)(1200)

/// Sends an UDP packet with the specified payload.
/// Maximum size is 1200, since the general recommendations are somewhere between 1200 and 1400 octets.
/// (https://www.ietf.org/id/draft-ietf-dnsop-avoid-fragmentation-06.html#section-3.3). Note: On MacOS the maximum size
/// is 9216 octets.
/// @param self
/// @param data the data to be sent
/// @param size the octet count of the data
/// @return negative number is an error code
int udpClientSend(UdpClientSocket* self, const uint8_t* data, size_t size)
{
    if (size > UDP_CLIENT_MAX_OCTET_SIZE) {
        CLOG_ERROR("You wanted to send %zu octets, but the recommended maximum size is %zu", size,
            UDP_CLIENT_MAX_OCTET_SIZE)
        return -2;
    }
    if (size == 0) {
        CLOG_SOFT_ERROR("udpClientSend: you can not send zero length packets")
    }
    ssize_t number_of_octets_sent = sendto(self->handle, data, size, 0, (struct sockaddr*)&self->peer_address,
        sizeof(self->peer_address));

    if (number_of_octets_sent < 0) {
        CLOG_WARN("Error send! errno:%d return: %ld\n", UDP_CLIENT_GET_ERROR, number_of_octets_sent)
        return -1;
    }

    return ((size_t)number_of_octets_sent == size);
}

/// Try to receive an UDP packet
/// If not available, it returns zero.
/// @param self
/// @param data
/// @param size
/// @return returns zero if it would block, or if no packet is available, negative numbers for error. positive numbers is the number of octets in the payload.
int udpClientReceive(UdpClientSocket* self, uint8_t* data, size_t size)
{
    struct sockaddr_in from_who;

    socklen_t addr_size = sizeof(from_who);
    ssize_t number_of_octets = recvfrom(self->handle, data, size, 0, (struct sockaddr*)&from_who, &addr_size);
    if (number_of_octets == -1) {
        int last_err = UDP_CLIENT_GET_ERROR;
        if (last_err == UDP_CLIENT_ERROR_AGAIN || last_err == UDP_CLIENT_ERROR_WOULDBLOCK) {
            return 0;
        } else {
            CLOG_WARN("udpClientReceive: error %d\n", last_err);
            return 0;
        }
    }

    return (int)number_of_octets;
}
