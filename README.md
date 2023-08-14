# UDP Client

Minimal implementation of a non-blocking UDP Client.

Enforces a maximum datagram size of 1200 octets (`UDP_CLIENT_MAX_OCTET_SIZE`), to make it as compatible as possible with other datagram transports and to avoid datagram fragmentation over the Internet.
