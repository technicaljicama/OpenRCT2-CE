#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#ifndef DISABLE_NETWORK

#include <cmath>
#include <chrono>
#include <future>
#include <string>
#include <thread>

// clang-format off
#ifdef _WIN32
    // winsock2 must be included before windows.h
    #include <winsock2.h>
    #include <ws2tcpip.h>

    #define LAST_SOCKET_ERROR() WSAGetLastError()
    #undef EWOULDBLOCK
    #define EWOULDBLOCK WSAEWOULDBLOCK
    #ifndef SHUT_RD
        #define SHUT_RD SD_RECEIVE
    #endif
    #ifndef SHUT_RDWR
        #define SHUT_RDWR SD_BOTH
    #endif
    #define FLAG_NO_PIPE 0
#else
    #include <cerrno>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/tcp.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <fcntl.h>
    #include "../common.h"
    using SOCKET = sint32;
    #define SOCKET_ERROR -1
    #define INVALID_SOCKET -1
    #define LAST_SOCKET_ERROR() errno
    #define closesocket close
    #define ioctlsocket ioctl
    #if defined(__linux__)
        #define FLAG_NO_PIPE MSG_NOSIGNAL
    #else
        #define FLAG_NO_PIPE 0
    #endif // defined(__linux__)
#endif // _WIN32
// clang-format on

#include "TcpSocket.h"

constexpr auto CONNECT_TIMEOUT = std::chrono::milliseconds(3000);

#ifdef _WIN32
    static bool _wsaInitialised = false;
#endif

#if 0

extern "C" {
int getnameinfo(const struct sockaddr *restrict sa, socklen_t sl,
	char *restrict node, socklen_t nodelen,
	char *restrict serv, socklen_t servlen,
	int flags);
int getnameinfo(const struct sockaddr *restrict sa, socklen_t sl,
	char *restrict node, socklen_t nodelen,
	char *restrict serv, socklen_t servlen,
	int flags)
{
	char ptr[PTR_MAX];
	char buf[256], num[3*sizeof(int)+1];
	int af = sa->sa_family;
	unsigned char *a;
	unsigned scopeid;

	switch (af) {
	case AF_INET:
		a = (void *)&((struct sockaddr_in *)sa)->sin_addr;
		if (sl < sizeof(struct sockaddr_in)) return EAI_FAMILY;
		mkptr4(ptr, a);
		scopeid = 0;
		break;
	case AF_INET6:
		a = (void *)&((struct sockaddr_in6 *)sa)->sin6_addr;
		if (sl < sizeof(struct sockaddr_in6)) return EAI_FAMILY;
		if (memcmp(a, "\0\0\0\0\0\0\0\0\0\0\xff\xff", 12))
			mkptr6(ptr, a);
		else
			mkptr4(ptr, a+12);
		scopeid = ((struct sockaddr_in6 *)sa)->sin6_scope_id;
		break;
	default:
		return EAI_FAMILY;
	}

	if (node && nodelen) {
		buf[0] = 0;
		if (!(flags & NI_NUMERICHOST)) {
			reverse_hosts(buf, a, scopeid, af);
		}
		if (!*buf && !(flags & NI_NUMERICHOST)) {
			unsigned char query[18+PTR_MAX], reply[512];
			int qlen = __res_mkquery(0, ptr, 1, RR_PTR,
				0, 0, 0, query, sizeof query);
			query[3] = 0; /* don't need AD flag */
			int rlen = __res_send(query, qlen, reply, sizeof reply);
			buf[0] = 0;
			if (rlen > 0) {
				if (rlen > sizeof reply) rlen = sizeof reply;
				__dns_parse(reply, rlen, dns_parse_callback, buf);
			}
		}
		if (!*buf) {
			if (flags & NI_NAMEREQD) return EAI_NONAME;
			inet_ntop(af, a, buf, sizeof buf);
			if (scopeid) {
				char *p = 0, tmp[IF_NAMESIZE+1];
				if (!(flags & NI_NUMERICSCOPE) &&
				    (IN6_IS_ADDR_LINKLOCAL(a) ||
				     IN6_IS_ADDR_MC_LINKLOCAL(a)))
					p = if_indextoname(scopeid, tmp+1);
				if (!p)
					p = itoa(num, scopeid);
				*--p = '%';
				strcat(buf, p);
			}
		}
		if (strlen(buf) >= nodelen) return EAI_OVERFLOW;
		strcpy(node, buf);
	}

	if (serv && servlen) {
		char *p = buf;
		int port = ntohs(((struct sockaddr_in *)sa)->sin_port);
		buf[0] = 0;
		if (!(flags & NI_NUMERICSERV))
			reverse_services(buf, port, flags & NI_DGRAM);
		if (!*p)
			p = itoa(num, port);
		if (strlen(p) >= servlen)
			return EAI_OVERFLOW;
		strcpy(serv, p);
	}

	return 0;
}
}
#endif
extern "C" {
    int custom_getnameinfo(const struct sockaddr *sa, socklen_t salen,
                       char *host, size_t hostlen,
                       char *serv, size_t servlen, int flags);
int custom_getnameinfo(const struct sockaddr *sa, socklen_t salen,
                       char *host, size_t hostlen,
                       char *serv, size_t servlen, int flags) {
    if (sa == NULL) {
        return EAI_FAIL;
    }

    if (host != NULL) {
        memset(host, 0, hostlen);
    }

    if (serv != NULL) {
        memset(serv, 0, servlen);
    }

    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in *sa_in = (const struct sockaddr_in *)sa;
        if (host != NULL) {
            if (inet_ntop(AF_INET, &sa_in->sin_addr, host, hostlen) == NULL) {
                return EAI_SYSTEM;
            }
            if (!(flags & NI_NUMERICHOST)) {
                struct hostent *he = gethostbyaddr((const void *)&sa_in->sin_addr, sizeof(sa_in->sin_addr), AF_INET);
                if (he != NULL) {
                    strncpy(host, he->h_name, hostlen - 1);
                }
            }
        }

        if (serv != NULL) {
            if (!(flags & NI_NUMERICSERV)) {
                printf("not supported\n");
            } else {
                snprintf(serv, servlen, "%d", ntohs(sa_in->sin_port));
            }
        }
    } else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sa_in6 = (const struct sockaddr_in6 *)sa;
        if (host != NULL) {
            if (inet_ntop(AF_INET6, &sa_in6->sin6_addr, host, hostlen) == NULL) {
                return EAI_SYSTEM;
            }
            if (!(flags & NI_NUMERICHOST)) {
                struct hostent *he = gethostbyaddr((const void *)&sa_in6->sin6_addr, sizeof(sa_in6->sin6_addr), AF_INET6);
                if (he != NULL) {
                    strncpy(host, he->h_name, hostlen - 1);
                }
            }
        }

        if (serv != NULL) {
            if (!(flags & NI_NUMERICSERV)) {
                printf("not supported\n");
            } else {
                snprintf(serv, servlen, "%d", ntohs(sa_in6->sin6_port));
            }
        }
    } else {
        return EAI_FAMILY;
    }

    return 0; // Success
}
}
class TcpSocket;

class SocketException : public std::runtime_error
{
public:
    explicit SocketException(const std::string &message) : std::runtime_error(message) { }
};

class TcpSocket final : public ITcpSocket
{
private:
    SOCKET_STATUS   _status         = SOCKET_STATUS_CLOSED;
    uint16          _listeningPort  = 0;
    SOCKET          _socket         = INVALID_SOCKET;

    std::string         _hostName;
    std::future<void>   _connectFuture;
    std::string         _error;

public:
    TcpSocket()
    {
    }

    ~TcpSocket() override
    {
        if (_connectFuture.valid())
        {
            _connectFuture.wait();
        }
        CloseSocket();
    }

    SOCKET_STATUS GetStatus() override
    {
        return _status;
    }

    const char * GetError() override
    {
        return _error.empty() ? nullptr : _error.c_str();
    }

    void Listen(uint16 port) override
    {
        Listen(nullptr, port);
    }

    void Listen(const char * address, uint16 port) override
    {
        if (_status != SOCKET_STATUS_CLOSED)
        {
            throw std::runtime_error("Socket not closed.");
        }

        sockaddr_storage ss;
        sint32 ss_len;
        if (!ResolveAddress(address, port, &ss, &ss_len))
        {
            throw SocketException("Unable to resolve address.");
        }

        // Create the listening socket
        _socket = socket(ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
        if (_socket == INVALID_SOCKET)
        {
            throw SocketException("Unable to create socket.");
        }

        // Turn off IPV6_V6ONLY so we can accept both v4 and v6 connections
        sint32 value = 0;
#ifndef __psp2__
        if (setsockopt(_socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&value, sizeof(value)) != 0)
        {
            log_error("IPV6_V6ONLY failed. %d", LAST_SOCKET_ERROR());
        }
#else
#define SOMAXCONN 128
#endif
        value = 1;
        if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&value, sizeof(value)) != 0)
        {
            log_error("SO_REUSEADDR failed. %d", LAST_SOCKET_ERROR());
        }

        try
        {
            // Bind to address:port and listen
            if (bind(_socket, (sockaddr *)&ss, ss_len) != 0)
            {
                throw SocketException("Unable to bind to socket.");
            }
            if (listen(_socket, SOMAXCONN) != 0)
            {
                throw SocketException("Unable to listen on socket.");
            }

            if (!SetNonBlocking(_socket, true))
            {
                throw SocketException("Failed to set non-blocking mode.");
            }
        }
        catch (const std::exception &)
        {
            CloseSocket();
            throw;
        }

        _listeningPort = port;
        _status = SOCKET_STATUS_LISTENING;
    }

    ITcpSocket * Accept() override
    {
        if (_status != SOCKET_STATUS_LISTENING)
        {
            throw std::runtime_error("Socket not listening.");
        }
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(struct sockaddr_storage);

        ITcpSocket * tcpSocket = nullptr;
        SOCKET socket = accept(_socket, (struct sockaddr *)&client_addr, &client_len);
        if (socket == INVALID_SOCKET)
        {
            if (LAST_SOCKET_ERROR() != EWOULDBLOCK)
            {
                log_error("Failed to accept client.");
            }
        }
        else
        {
            if (!SetNonBlocking(socket, true))
            {
                closesocket(socket);
                log_error("Failed to set non-blocking mode.");
            }
            else
            {
                char hostName[NI_MAXHOST];
                sint32 rc = custom_getnameinfo(
                    (struct sockaddr *)&client_addr,
                    client_len,
                    hostName,
                    sizeof(hostName),
                    nullptr,
                    0,
                    NI_NUMERICHOST | NI_NUMERICSERV);
                SetTCPNoDelay(socket, true);
                tcpSocket = new TcpSocket(socket);
                if (rc == 0)
                {
                    _hostName = std::string(hostName);
                }
            }
        }
        return tcpSocket;
    }

    void Connect(const char * address, uint16 port) override
    {
        if (_status != SOCKET_STATUS_CLOSED)
        {
            throw std::runtime_error("Socket not closed.");
        }

        try
        {
            // Resolve address
            _status = SOCKET_STATUS_RESOLVING;

            sockaddr_storage ss;
            sint32 ss_len = 0;
            if (!ResolveAddress(address, port, &ss, &ss_len))
            {
                throw SocketException("Unable to resolve address.");
            }

            _status = SOCKET_STATUS_CONNECTING;
            _socket = socket(ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
            if (_socket == INVALID_SOCKET)
            {
                throw SocketException("Unable to create socket.");
            }

            SetTCPNoDelay(_socket, true);
            if (!SetNonBlocking(_socket, true))
            {
                throw SocketException("Failed to set non-blocking mode.");
            }

            // Connect
            sint32 connectResult = connect(_socket, (sockaddr *)&ss, ss_len);
            if (connectResult != SOCKET_ERROR || (LAST_SOCKET_ERROR() != EINPROGRESS &&
                                                  LAST_SOCKET_ERROR() != EWOULDBLOCK))
            {
                throw SocketException("Failed to connect.");
            }

            auto connectStartTime = std::chrono::system_clock::now();

            sint32 error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(_socket, SOL_SOCKET, SO_ERROR, (char *)&error, &len) != 0)
            {
                throw SocketException("getsockopt failed with error: " + std::to_string(LAST_SOCKET_ERROR()));
            }
            if (error != 0)
            {
                throw SocketException("Connection failed: " + std::to_string(error));
            }

            do
            {
                // Sleep for a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                fd_set writeFD;
                FD_ZERO(&writeFD);
#pragma warning(push)
#pragma warning(disable : 4548) // expression before comma has no effect; expected expression with side-effect
                FD_SET(_socket, &writeFD);
#pragma warning(pop)
                timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;
                if (select((sint32)(_socket + 1), nullptr, &writeFD, nullptr, &timeout) > 0)
                {
                    error = 0;
                    len = sizeof(error);
                    if (getsockopt(_socket, SOL_SOCKET, SO_ERROR, (char*)&error, &len) != 0)
                    {
                        throw SocketException("getsockopt failed with error: " + std::to_string(LAST_SOCKET_ERROR()));
                    }
                    if (error == 0)
                    {
                        _status = SOCKET_STATUS_CONNECTED;
                        return;
                    }
                }
            } while ((std::chrono::system_clock::now() - connectStartTime) < CONNECT_TIMEOUT);

            // Connection request timed out
            throw SocketException("Connection timed out.");
        }
        catch (const std::exception &)
        {
            CloseSocket();
            throw;
        }
    }

    void ConnectAsync(const char * address, uint16 port) override
    {
        if (_status != SOCKET_STATUS_CLOSED)
        {
            throw std::runtime_error("Socket not closed.");
        }

        auto saddress = std::string(address);
        std::promise<void> barrier;
        _connectFuture = barrier.get_future();
        auto thread = std::thread([this, saddress, port](std::promise<void> barrier2) -> void
        {
            try
            {
                Connect(saddress.c_str(), port);
            }
            catch (const std::exception &ex)
            {
                _error = std::string(ex.what());
            }
            barrier2.set_value();
        }, std::move(barrier));
        thread.detach();
    }

    void Disconnect() override
    {
        if (_status == SOCKET_STATUS_CONNECTED)
        {
            shutdown(_socket, SHUT_RDWR);
        }
    }

    size_t SendData(const void * buffer, size_t size) override
    {
        if (_status != SOCKET_STATUS_CONNECTED)
        {
            throw std::runtime_error("Socket not connected.");
        }

        size_t totalSent = 0;
        do
        {
            const char * bufferStart = (const char *)buffer + totalSent;
            size_t remainingSize = size - totalSent;
            sint32 sentBytes = send(_socket, bufferStart, (sint32)remainingSize, FLAG_NO_PIPE);
            if (sentBytes == SOCKET_ERROR)
            {
                return totalSent;
            }
            totalSent += sentBytes;
        } while (totalSent < size);
        return totalSent;
    }

    NETWORK_READPACKET ReceiveData(void * buffer, size_t size, size_t * sizeReceived) override
    {
        if (_status != SOCKET_STATUS_CONNECTED)
        {
            throw std::runtime_error("Socket not connected.");
        }

        sint32 readBytes = recv(_socket, (char *)buffer, (sint32)size, 0);
        if (readBytes == 0)
        {
            *sizeReceived = 0;
            return NETWORK_READPACKET_DISCONNECTED;
        }
        else if (readBytes == SOCKET_ERROR)
        {
            *sizeReceived = 0;
            if (LAST_SOCKET_ERROR() != EWOULDBLOCK && LAST_SOCKET_ERROR() != EAGAIN)
            {
                return NETWORK_READPACKET_DISCONNECTED;
            }
            else
            {
                return NETWORK_READPACKET_NO_DATA;
            }
        }
        else
        {
            *sizeReceived = readBytes;
            return NETWORK_READPACKET_SUCCESS;
        }
    }

    void Close() override
    {
        if (_connectFuture.valid())
        {
            _connectFuture.wait();
        }
        CloseSocket();
    }

    const char * GetHostName() const override
    {
        return _hostName.empty() ? nullptr : _hostName.c_str();
    }

private:
    explicit TcpSocket(SOCKET socket)
    {
        _socket = socket;
        _status = SOCKET_STATUS_CONNECTED;
    }

    void CloseSocket()
    {
        if (_socket != INVALID_SOCKET)
        {
            closesocket(_socket);
            _socket = INVALID_SOCKET;
        }
        _status = SOCKET_STATUS_CLOSED;
    }

    bool ResolveAddress(const char * address, uint16 port, sockaddr_storage * ss, sint32 * ss_len)
    {
        std::string serviceName = std::to_string(port);

        addrinfo hints = { 0 };
        hints.ai_family = AF_UNSPEC;
        if (address == nullptr)
        {
            hints.ai_flags = AI_PASSIVE;
        }

        addrinfo * result = nullptr;
        int errorcode = getaddrinfo(address, serviceName.c_str(), &hints, &result);
        if (errorcode != 0)
        {
            log_error("Resolving address failed: Code %d.", errorcode);
#ifndef __psp2__
            log_error("Resolution error message: %s.", gai_strerror(errorcode));
#endif
            return false;
        }
        if (result == nullptr)
        {
            return false;
        }
        else
        {
            memcpy(ss, result->ai_addr, result->ai_addrlen);
            *ss_len = (sint32)result->ai_addrlen;
            freeaddrinfo(result);
            return true;
        }
    }

    static bool SetNonBlocking(SOCKET socket, bool on)
    {
#ifdef _WIN32
        u_long nonBlocking = on;
        return ioctlsocket(socket, FIONBIO, &nonBlocking) == 0;
#else
        sint32 flags = fcntl(socket, F_GETFL, 0);
        return fcntl(socket, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == 0;
#endif
    }

    static bool SetTCPNoDelay(SOCKET socket, bool enabled)
    {
        return setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&enabled, sizeof(enabled)) == 0;
    }
};

ITcpSocket * CreateTcpSocket()
{
    return new TcpSocket();
}

bool InitialiseWSA()
{
#ifdef _WIN32
    if (!_wsaInitialised)
    {
        log_verbose("Initialising WSA");
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            log_error("Unable to initialise winsock.");
            return false;
        }
        _wsaInitialised = true;
    }
    return _wsaInitialised;
#else
    return true;
#endif
}

void DisposeWSA()
{
#ifdef _WIN32
    if (_wsaInitialised)
    {
        WSACleanup();
        _wsaInitialised = false;
    }
#endif
}

namespace Convert
{
    uint16 HostToNetwork(uint16 value)
    {
        return htons(value);
    }

    uint16 NetworkToHost(uint16 value)
    {
        return ntohs(value);
    }
}

#endif
