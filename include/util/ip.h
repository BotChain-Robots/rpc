#ifndef IP_UTIL_H
#define IP_UTIL_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSE_SOCKET closesocket
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET close
typedef int socket_t;
#endif

bool is_valid_ipv4(const std::string &ip) {
    struct in_addr addr;
    return inet_pton(AF_INET, ip.c_str(), &addr) == 1;
}

#endif // IP_UTIL_H
