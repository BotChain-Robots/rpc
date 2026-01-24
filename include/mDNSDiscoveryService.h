//
// Created by Johnathon Slightham on 2025-06-10.
//

#ifndef MDNSDISCOVERYSERVICE_H
#define MDNSDISCOVERYSERVICE_H

#include <chrono>
#include <unordered_map>

#include "BlockingQueue.h"
#include "ICommunicationClient.h"
#include "IDiscoveryService.h"
#include "mDNSRobotModule.h"

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

class mDNSDiscoveryService final : public IDiscoveryService {

  public:
    mDNSDiscoveryService();
    ~mDNSDiscoveryService() override;
    std::unordered_set<uint8_t> find_modules(std::chrono::duration<double> wait_time) override;
    std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> get_lossy_clients(
        const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
        std::vector<uint8_t> &skip_modules) override;
    std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> get_lossless_clients(
        const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
        std::vector<uint8_t> &skip_modules) override;

  private:
    template <typename T>
    std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> create_clients(
        const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
        std::vector<uint8_t> &skip_modules);
    static void send_mdns_query(socket_t sock, const sockaddr_in &addr);
    static std::optional<mDNSRobotModule> parse_response(uint8_t *buffer, int size);
    static std::tuple<std::string, int> read_mdns_name(const uint8_t *buffer, int size, int ptr);

    std::unordered_map<uint8_t, mDNSRobotModule> module_to_mdns{};
};

#endif // MDNSDISCOVERYSERVICE_H
