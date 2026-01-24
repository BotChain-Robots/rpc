//
// Created by Johnathon Slightham on 2025-06-10.
//

#ifndef IDISCOVERYSERVICE_H
#define IDISCOVERYSERVICE_H
#include <unordered_set>

#include "ICommunicationClient.h"
#include "mDNSRobotModule.h"

class IDiscoveryService {
  public:
    virtual ~IDiscoveryService() = default;
    virtual std::unordered_set<uint8_t> find_modules(std::chrono::duration<double> wait_time) = 0;
    virtual std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> get_lossy_clients(
        const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
        std::vector<uint8_t> &skip_modules) = 0;
    virtual std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> get_lossless_clients(
        const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
        std::vector<uint8_t> &skip_modules) = 0;
};

#endif // IDISCOVERYSERVICE_H
