#include "librpc.h"
#include "flatbuffers_generated/ReturnCall_generated.h"

#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <vector>

#undef min
#include <algorithm>

#include "flatbuffers/CallBuilder.h"
#include "flatbuffers/MPIMessageBuilder.h"
#include "spdlog/spdlog.h"

constexpr auto MAX_RECV_WAIT_TIME = std::chrono::seconds(3);
constexpr auto PER_TAG_MAX_QUEUE_SIZE = 50;
constexpr auto MAX_WAIT_TIME_TAG_ENQUEUE = std::chrono::milliseconds(250);
constexpr auto MAX_WAIT_TIME_RX_THREAD_DEQUEUE = std::chrono::milliseconds(250);
constexpr auto FN_RETURN_BUFFER_SIZE = 1024;

MessagingInterface::~MessagingInterface() {
    m_stop_flag = true;
    m_rx_thread.join();
    m_fn_rx_thread.join();

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
    std::unique_lock lock(m_tag_queue_mutex);
    if (!m_tag_to_queue_map.contains(tag)) {
        m_tag_to_queue_map.insert(
            {tag, std::make_unique<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>>(
                      PER_TAG_MAX_QUEUE_SIZE)});
    }
    lock.unlock();

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
    // Cannot just skip the call if already running, since the caller needs the
    // list of modules.
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

            std::unique_lock lock(m_tag_queue_mutex);
            if (!m_tag_to_queue_map.contains(mpi_message->tag())) {
                m_tag_to_queue_map.insert(
                    {mpi_message->tag(),
                     std::make_unique<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>>(
                         PER_TAG_MAX_QUEUE_SIZE)});
            }
            lock.unlock();

            m_tag_to_queue_map[mpi_message->tag()]->enqueue(std::move(data.value()),
                                                            MAX_WAIT_TIME_TAG_ENQUEUE);
        }
    }
}

std::optional<std::unique_ptr<std::vector<uint8_t>>>
MessagingInterface::remote_call(uint8_t function_tag, uint8_t module_id,
                                const std::vector<uint8_t> &parameters) {
    std::unique_lock lock(m_fn_call_mutex);
    const auto unique_id = unique_fn_call_id++;
    auto sem = std::make_unique<std::counting_semaphore<1>>(0);
    m_fn_call_to_semaphore.insert_or_assign(unique_id, std::move(sem));
    lock.unlock();

    Flatbuffers::CallBuilder builder{};
    auto [data, size] = builder.build_send_call(function_tag, unique_id, parameters);

    // Assume durable transmission, non-durable RPC calls do not make sense.
    // If a message is lost, we will block unnecessarily until the timeout.
    // todo: is this thread safe? especially if other threads will be calling send
    // themselves.
    send((uint8_t *)data, size, module_id, FN_CALL_TAG, true);

    if (m_fn_call_to_semaphore[unique_id]->try_acquire_for(FN_CALL_TIMEOUT)) {
        lock.lock();

        if (!m_fn_call_to_result[unique_id]) {
            m_fn_call_to_semaphore.erase(unique_id);
            return std::nullopt;
        }

        auto result = std::move(m_fn_call_to_result[unique_id]);
        m_fn_call_to_result.erase(unique_id);
        m_fn_call_to_semaphore.erase(unique_id);
        return std::move(result);
    }

    lock.lock();
    m_fn_call_to_semaphore.erase(unique_id);

    return std::nullopt;
}

void MessagingInterface::handle_fn_recv() {
    while (!m_stop_flag) {
        auto buffer = std::make_unique<std::vector<uint8_t>>();
        buffer->resize(FN_RETURN_BUFFER_SIZE);

        std::optional<SizeAndSource> maybe_result;
        SizeAndSource result;
        do {
            maybe_result = recv(buffer->data(), FN_RETURN_BUFFER_SIZE, FN_CALL_TAG);
        } while (!m_stop_flag && !maybe_result);
        if (m_stop_flag) {
            return;
        }

        Flatbuffers::CallBuilder builder{};
        std::unique_lock lock(m_fn_call_mutex);
        result = *maybe_result;

        flatbuffers::Verifier verifier(buffer->data(), result.bytes_written);
        bool ok = Messaging::VerifyReturnCallBuffer(verifier);
        if (!ok) {
            spdlog::error("[LibRPC] Got an invalid return buffer");
            continue;
        }

        const auto return_data = builder.parse_return_call(buffer->data());
        if (return_data->length() > FN_RETURN_BUFFER_SIZE) {
            spdlog::warn("[LibRPC] Got a return buffer with return data that is too large");
            continue;
        }

        auto it = m_fn_call_to_semaphore.find(return_data->unique_id());
        if (it == m_fn_call_to_semaphore.end() || !it->second) {
            spdlog::warn("[LibRPC] Previously timed out RPC call completed, "
                         "discarding result");
            continue;
        }

        auto raw_data = std::make_unique<std::vector<uint8_t>>();
        raw_data->resize(return_data->length());
        std::memcpy(raw_data->data(), return_data->return_value()->data(), return_data->length());
        m_fn_call_to_result[return_data->unique_id()] = std::move(raw_data);
        it->second->release();
    }
}
