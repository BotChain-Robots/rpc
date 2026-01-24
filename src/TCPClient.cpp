//
// Created by Johnathon Slightham on 2025-06-10.
//

#include <chrono>
#include <iostream>
#include <vector>

#include "TCPClient.h"
#include "constants.h"
#include "spdlog/spdlog.h"

constexpr auto SLEEP_WHILE_INITIALIZING = std::chrono::milliseconds(250);
constexpr int PORT = 3001;
constexpr auto QUEUE_ADD_TIMEOUT = std::chrono::milliseconds(100);
constexpr auto RX_SLEEP_ON_ERROR = std::chrono::milliseconds(100);
constexpr auto SOCKET_TIMEOUT_MS = 2500;

// todo: - add authentication
//       - encryption

TCPClient::~TCPClient() {
    this->m_stop_flag = true;
    this->m_thread.join();
    this->deinit();
}

int TCPClient::init() {
    sockaddr_in serv_addr{};

    if ((this->m_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        spdlog::error("[TCP] Failed to create socket");
        return -2;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, this->m_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        spdlog::error("[TCP] Invalid address");
        deinit();
        return -1;
    }

    timeval timeout{};
    timeout.tv_sec = SOCKET_TIMEOUT_MS / 1000;
    timeout.tv_usec = (SOCKET_TIMEOUT_MS % 1000) * 1000;

#ifdef _WIN32
    setsockopt(this->m_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(this->m_socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
#else
    setsockopt(this->m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(this->m_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

    if (connect(this->m_socket, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0) {
        spdlog::error("[TCP] Connection failed");
        deinit();
        return -1;
    }

    this->m_initialized = true;
    return 0;
}

void TCPClient::deinit() {
    this->m_initialized = false;
    if (this->m_socket > 0) {
        CLOSE_SOCKET(this->m_socket);
        this->m_socket = -1;
    }
}

int TCPClient::send_msg(void *sendbuff, const uint32_t len) {
    if (!m_initialized) {
        return -1;
    }

    if (send(this->m_socket, (char *)&len, 4, 0) < 4) {
        return -1;
    }

    return send(this->m_socket, (char *)sendbuff, len, 0);
}

void TCPClient::rx_thread() const {
    while (!m_stop_flag) {
        if (!m_initialized) {
            std::this_thread::sleep_for(SLEEP_WHILE_INITIALIZING);
            continue;
        }

        uint32_t data_len = 0;
        if (recv(this->m_socket, (char *)&data_len, 4, MSG_WAITALL) < 0) {
            std::this_thread::sleep_for(RX_SLEEP_ON_ERROR);
            continue;
        }

        if (data_len > MAX_BUFFER_SIZE || data_len < 1) {
            std::this_thread::sleep_for(RX_SLEEP_ON_ERROR);
            continue;
        }

        auto buffer = std::make_unique<std::vector<uint8_t>>();
        buffer->resize(MAX_BUFFER_SIZE);
        if (const auto read = recv(this->m_socket, (char *)buffer->data(), data_len, MSG_WAITALL);
            read > 0) {
            m_rx_queue->enqueue(std::move(buffer), QUEUE_ADD_TIMEOUT);
        } else {
            std::this_thread::sleep_for(RX_SLEEP_ON_ERROR);
        }
    }
}
