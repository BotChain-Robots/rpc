//
// Created by Johnathon Slightham on 2025-12-27.
//

#ifndef UDPCLIENT_H
#define UDPCLIENT_H
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

class UDPClient final : public ICommunicationClient {

  public:
    UDPClient(std::string /* ip */,
              const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue)
        : m_stop_flag(false), m_thread(std::thread(&UDPClient::rx_thread, this)),
          m_rx_queue(rx_queue) {
    }
    ~UDPClient() override;
    int init() override;
    int send_msg(void *sendbuff, uint32_t len) override;

  private:
    void deinit();
    void rx_thread() const;

    socket_t m_tx_socket = -1;
    socket_t m_rx_socket = -1;
    bool m_initialized = false;
    std::atomic<bool> m_stop_flag;
    std::thread m_thread;
    std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> m_rx_queue;
};

#endif // UDPCLIENT_H
