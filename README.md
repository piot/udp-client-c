# UDP Client

Minimal implementation of a non-blocking UDP Client.

Enforces a maximum datagram size of 1200 octets (`UDP_CLIENT_MAX_OCTET_SIZE`), to make it as compatible [^1] as possible with other datagram transports and to avoid datagram fragmentation over the Internet[^2].

[^1]: [Steam networking](https://partner.steamgames.com/doc/api/ISteamNetworking) has a 1200 octets maximum.
[^2]: [IETF draft](https://www.ietf.org/archive/id/draft-ietf-dnsop-avoid-fragmentation-07.html)
