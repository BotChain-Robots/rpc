//
// Created by Johnathon Slightham on 2025-06-10.
//

#ifndef TCPCLIENT_H
#define TCPCLIENT_H
#include <thread>
#include <utility>

#include "ICommunicationClient.h"

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

#include "BlockingQueue.h"

class TCPClient final : public ICommunicationClient {

  public:
    TCPClient(std::string ip,
              const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue)
        : port{3000}, m_ip{std::move(ip)}, m_stop_flag(false),
          m_thread(std::thread(&TCPClient::rx_thread, this)), m_rx_queue(rx_queue) {
    }
    ~TCPClient() override;
    int init() override;
    int send_msg(void *sendbuff, uint32_t len) override;

  private:
    void deinit();
    void rx_thread() const;

    socket_t m_socket = -1;
    int port;
    bool m_initialized = false;
    std::string m_ip;
    std::atomic<bool> m_stop_flag;
    std::thread m_thread;
    std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> m_rx_queue;
};

#endif // TCPCLIENT_H
