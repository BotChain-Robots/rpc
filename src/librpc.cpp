#include "librpc.h"

#include <mutex>
#include <optional>
#include <vector>

#undef min
#include <algorithm>

#include "flatbuffers/MPIMessageBuilder.h"
#include "spdlog/spdlog.h"

constexpr auto MAX_RECV_WAIT_TIME = std::chrono::seconds(3);
constexpr auto PER_TAG_MAX_QUEUE_SIZE = 50;
constexpr auto MAX_WAIT_TIME_TAG_ENQUEUE = std::chrono::milliseconds(250);
constexpr auto MAX_WAIT_TIME_RX_THREAD_DEQUEUE = std::chrono::milliseconds(250);

MessagingInterface::~MessagingInterface() {
    m_stop_flag = true;
    m_rx_thread.join();

#ifdef _WIN32
    WSACleanup();
#endif
}

int MessagingInterface::send(uint8_t *buffer, const size_t size, const uint8_t destination,
                             const uint8_t tag, const bool durable) {
    if (!this->m_id_to_lossless_client.contains(destination)) {
        return -1;
    }

    Flatbuffers::MPIMessageBuilder builder;
    const auto [mpi_buffer, mpi_size] = builder.build_mpi_message(
        Messaging::MessageType_PTP, PC_MODULE_ID, destination, m_sequence_number++, durable, tag,
        std::vector<uint8_t>(buffer, buffer + size));

    std::shared_lock lock(m_client_mutex);
    if (durable) {
        this->m_id_to_lossless_client[destination]->send_msg(mpi_buffer, mpi_size);
    } else {
        this->m_id_to_lossy_client[destination]->send_msg(mpi_buffer, mpi_size);
    }

    return 0;
}

int MessagingInterface::broadcast(uint8_t *buffer, size_t size, bool durable) {
    return -1; // todo
}

std::optional<SizeAndSource> MessagingInterface::recv(uint8_t *buffer, const size_t size,
                                                      uint8_t tag) {
    if (!m_tag_to_queue_map.contains(tag)) {
        m_tag_to_queue_map.insert(
            {tag, std::make_unique<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>>(
                      PER_TAG_MAX_QUEUE_SIZE)});
    }

    const auto data = m_tag_to_queue_map[tag]->dequeue(MAX_RECV_WAIT_TIME);

    if (!data.has_value()) {
        return std::nullopt;
    }

    // Anything in the queue should already be validated
    const auto mpi_message =
        Flatbuffers::MPIMessageBuilder::parse_mpi_message(data.value()->data());
    const auto data_size = std::min(size, static_cast<size_t>(mpi_message->length()));

    std::memcpy(buffer, mpi_message->payload()->data(), data_size);

    return std::make_optional<SizeAndSource>({data_size, mpi_message->sender()});
}

int MessagingInterface::sendrecv(uint8_t *send_buffer, size_t send_size, uint8_t dest,
                                 uint8_t send_tag, uint8_t *recv_buffer, size_t recv_size,
                                 uint8_t recv_tag) {
    // no-op
    return -1;
}

std::unordered_set<uint8_t>
MessagingInterface::find_connected_modules(const std::chrono::duration<double> scan_duration) {
    // Cannot just skip the call if already running, since the caller needs the list of modules.
    std::unique_lock scan_lock(m_scan_mutex);
    const auto foundModules = this->m_discovery_service->find_modules(scan_duration);
    scan_lock.unlock();

    std::unique_lock lock(m_client_mutex);

    std::vector<uint8_t> existing_clients;
    existing_clients.reserve(m_id_to_lossless_client.size());
    for (auto &kv : m_id_to_lossless_client) {
        existing_clients.push_back(kv.first);
    }

    const auto new_lossless =
        this->m_discovery_service->get_lossless_clients(m_rx_queue, existing_clients);
    const auto new_lossy =
        this->m_discovery_service->get_lossy_clients(m_rx_queue, existing_clients);

    m_id_to_lossless_client.insert(new_lossless.begin(), new_lossless.end());
    m_id_to_lossy_client.insert(new_lossy.begin(), new_lossy.end());

    return foundModules;
}

void MessagingInterface::handle_recv() {
    while (!m_stop_flag) {
        if (auto data = this->m_rx_queue->dequeue(MAX_WAIT_TIME_RX_THREAD_DEQUEUE);
            data.has_value()) {
            flatbuffers::Verifier verifier(data.value()->data(), data.value()->size());
            bool ok = Messaging::VerifyMPIMessageBuffer(verifier);
            if (!ok) {
                spdlog::error("[LibRPC] Got invalid flatbuffer data");
                continue;
            }

            const auto &mpi_message =
                Flatbuffers::MPIMessageBuilder::parse_mpi_message(data.value()->data());

            if (!m_tag_to_queue_map.contains(mpi_message->tag())) {
                m_tag_to_queue_map.insert(
                    {mpi_message->tag(),
                     std::make_unique<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>>(
                         PER_TAG_MAX_QUEUE_SIZE)});
            }

            m_tag_to_queue_map[mpi_message->tag()]->enqueue(std::move(data.value()),
                                                            MAX_WAIT_TIME_TAG_ENQUEUE);
        }
    }
}
