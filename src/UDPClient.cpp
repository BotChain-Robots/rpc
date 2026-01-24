//
// Created by Johnathon Slightham on 2025-06-10.
//

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include "UDPClient.h"
#include "spdlog/spdlog.h"
#include "util/log.h"

constexpr auto SLEEP_WHILE_INITIALIZING = std::chrono::milliseconds(250);
constexpr int TX_PORT = 3101;
constexpr int RX_PORT = 3100;
constexpr std::string RECV_MCAST = "239.1.1.2";
constexpr std::string SEND_MCAST = "239.1.1.1";
constexpr auto SOCKET_TIMEOUT_MS = 2500;
constexpr auto QUEUE_ADD_TIMEOUT = std::chrono::milliseconds(100);
constexpr auto RX_SLEEP_ON_ERROR = std::chrono::milliseconds(100);
constexpr auto RX_BUFFER_SIZE = 1024;

// todo: - add authentication
//       - encryption

UDPClient::~UDPClient() {
    this->m_stop_flag = true;
    this->m_thread.join();
    this->deinit();
}

int UDPClient::init() {
    if ((this->m_rx_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        spdlog::error("[UDP] Failed to create socket");
        print_errno();
        return -2;
    }

    if ((this->m_tx_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        spdlog::error("[UDP] Failed to create socket");
        print_errno();
        deinit();
        return -2;
    }

    timeval timeout{};
    timeout.tv_sec = SOCKET_TIMEOUT_MS / 1000;
    timeout.tv_usec = (SOCKET_TIMEOUT_MS % 1000) * 1000;

#ifdef _WIN32
    setsockopt(this->m_rx_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(this->m_tx_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
#else
    setsockopt(this->m_rx_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(this->m_tx_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

    sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(RX_PORT),
    };
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (int err = bind(m_rx_socket, reinterpret_cast<struct sockaddr *>(&server_addr),
                       sizeof(server_addr));
        0 != err) {
        spdlog::error("[UDP] Socket unable to bind");
        print_errno();
        deinit();
        return -1;
    }

    constexpr int opt = 1;
#ifdef _WIN32
    setsockopt(m_rx_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    setsockopt(m_tx_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
    setsockopt(m_rx_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(m_tx_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(RECV_MCAST.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;

#ifdef _WIN32
    // Get hostname, resolve to primary IPv4 (won't work for all cases)
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    hostent *host = gethostbyname(hostname);
    if (host && host->h_addr_list[0]) {
        mreq.imr_interface.s_addr = *(uint32_t *)host->h_addr_list[0];
    } else {
        mreq.imr_interface.s_addr = INADDR_ANY; // Fallback
    }

    spdlog::info("[UDP] Listening on {}", mreq.imr_interface.s_addr);

    if (setsockopt(m_rx_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
        spdlog::error("[UDP] Failed to join multicast group");
        print_errno();
        deinit();
        return -1;
    }
#else
    setsockopt(m_rx_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
#endif

    this->m_initialized = true;

    return 0;
}

void UDPClient::deinit() {
    this->m_initialized = false;

    if (this->m_tx_socket > 0) {
        CLOSE_SOCKET(this->m_tx_socket);
        this->m_tx_socket = -1;
    }

    if (this->m_rx_socket > 0) {
        CLOSE_SOCKET(this->m_rx_socket);
        this->m_rx_socket = -1;
    }
}

int UDPClient::send_msg(void *sendbuff, const uint32_t len) {
    if (!m_initialized) {
        return -1;
    }

    std::vector<uint8_t> buffer;
    buffer.resize(len + 4);

    *reinterpret_cast<uint32_t *>(buffer.data()) = static_cast<uint32_t>(len);
    std::memcpy(buffer.data() + 4, sendbuff, len);

    sockaddr_in mcast_dest = {
        .sin_family = AF_INET,
        .sin_port = htons(TX_PORT),
    };
    inet_pton(AF_INET, SEND_MCAST.c_str(), &mcast_dest.sin_addr);

#ifdef _WIN32
    return sendto(m_tx_socket, reinterpret_cast<const char *>(buffer.data()), buffer.size(), 0,
                  reinterpret_cast<sockaddr *>(&mcast_dest), sizeof(mcast_dest));
#else
    return sendto(m_tx_socket, buffer.data(), buffer.size(), 0,
                  reinterpret_cast<sockaddr *>(&mcast_dest), sizeof(mcast_dest));
#endif
}

void UDPClient::rx_thread() const {

    while (!m_stop_flag) {
        if (!m_initialized) {
            std::this_thread::sleep_for(RX_SLEEP_ON_ERROR);
            continue;
        }

        auto buffer = std::make_unique<std::vector<uint8_t>>();
        buffer->resize(RX_BUFFER_SIZE);

#ifdef _WIN32
        const auto len = recv(m_rx_socket, (char *)buffer->data(), RX_BUFFER_SIZE, 0);
#else
        const auto len = recv(m_rx_socket, buffer->data(), RX_BUFFER_SIZE, 0);
#endif
        if (len < 0) {
            std::this_thread::sleep_for(RX_SLEEP_ON_ERROR);
        } else if (len < 4 || len > RX_BUFFER_SIZE) {
            spdlog::error("[UDP] Message size of {} incorrect", len);
        } else {
            uint32_t msg_size = *reinterpret_cast<uint32_t *>(buffer->data());
            if (msg_size > len - 4) {
                spdlog::error("[UDP] Message size incorrect {}", msg_size);
                continue;
            }

            buffer->erase(buffer->begin(), buffer->begin() + 4);
            buffer->resize(msg_size);
            m_rx_queue->enqueue(std::move(buffer), QUEUE_ADD_TIMEOUT);
        }
    }
}
