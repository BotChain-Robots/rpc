#ifndef RPC_LIBRARY_H
#define RPC_LIBRARY_H

#include <chrono>
#include <memory>
#include <semaphore>
#include <shared_mutex>
#include <thread>

#include "BlockingQueue.h"
#include "constants.h"
#include "flatbuffers/CallBuilder.h"
#include "mDNSDiscoveryService.h"

constexpr auto RX_QUEUE_SIZE = 100;
constexpr auto FN_CALL_TAG = 100; // reserved tag for RPC functionality
constexpr auto FN_CALL_TIMEOUT = std::chrono::seconds(10);

struct SizeAndSource {
    size_t bytes_written;
    uint8_t sender;
};

class MessagingInterface {
  public:
    MessagingInterface()
        : m_stop_flag(false), m_rx_thread(std::thread(&MessagingInterface::handle_recv, this)),
          m_fn_rx_thread(std::thread(&MessagingInterface::handle_fn_recv, this)),
          m_rx_queue(std::make_shared<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>>(
              RX_QUEUE_SIZE)) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
        // Initialization must be after call to WSAStartup
        m_discovery_service = std::make_unique<mDNSDiscoveryService>();
    }

    ~MessagingInterface();
    int send(uint8_t *buffer, size_t size, uint8_t destination, uint8_t tag, bool durable);
    int broadcast(uint8_t *buffer, size_t size, bool durable); // todo
    std::optional<SizeAndSource> recv(uint8_t *buffer, size_t size, uint8_t tag);
    int sendrecv(uint8_t *send_buffer, size_t send_size, uint8_t dest, uint8_t send_tag,
                 uint8_t *recv_buffer, size_t recv_size,
                 uint8_t recv_tag); // todo
    std::optional<std::unique_ptr<std::vector<uint8_t>>>
    remote_call(uint8_t function_tag, uint8_t module, const std::vector<uint8_t> &parameters);
    std::unordered_set<uint8_t> find_connected_modules(std::chrono::duration<double> scan_duration);

  private:
    void handle_recv();
    void handle_fn_recv();

    uint16_t m_sequence_number = 0;
    uint8_t unique_fn_call_id = 0; // this is designed to overflow, change to uint16_t if we plan on
                                   // having way more calls per second.
    std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> m_id_to_lossless_client;
    std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> m_id_to_lossy_client;
    std::unordered_map<int, std::unique_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>>>
        m_tag_to_queue_map;
    // The semaphore needs to be in a unique_ptr, since it is not copyable or
    // movable unordered_maps need to copy/move to reshuffle.
    std::unordered_map<uint8_t, std::unique_ptr<std::binary_semaphore>> m_fn_call_to_semaphore;
    std::unordered_map<uint8_t, std::unique_ptr<std::vector<uint8_t>>> m_fn_call_to_result;
    std::unique_ptr<IDiscoveryService> m_discovery_service;
    std::atomic<bool> m_stop_flag;
    std::thread m_rx_thread;
    std::thread m_fn_rx_thread;
    std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> m_rx_queue;
    std::shared_mutex m_client_mutex;
    std::shared_mutex m_scan_mutex;
    std::mutex m_fn_call_mutex;
    std::mutex m_tag_queue_mutex;
};

#endif // RPC_LIBRARY_H
