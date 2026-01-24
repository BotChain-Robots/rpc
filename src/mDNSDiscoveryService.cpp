//
// Created by Johnathon Slightham on 2025-06-10.
//

#include <algorithm>
#include <cstring>
#include <iostream>
#include <optional>
#include <thread>

#include "TCPClient.h"
#include "mDNSDiscoveryService.h"

#include "UDPClient.h"
#include "spdlog/spdlog.h"
#include "util/ip.h"
#include "util/string.h"

#define MDNS_PORT 5353
#define MDNS_GROUP "224.0.0.251"
#define RECV_BLOCK_SIZE 1024
#define MODULE_TYPE_STR "module_type"
#define MODULE_ID_STR "module_id"
#define CONNECTED_MODULES_STR "connected_modules"

#pragma pack(push, 1) // prevent padding between struct members
struct query_header {
    uint16_t id;
    uint16_t flags;
    uint16_t num_questions;
    uint16_t num_answers;
    uint16_t num_authority;
    uint16_t num_additional;
};

struct query_footer { // footer for the question not for the packet
    uint16_t type = htons(0x00FF);
    uint16_t class_id = htons(0x8001);
};

struct answer {
    uint16_t type;
    uint16_t answer_class;
    uint32_t ttl;
    uint16_t data_length;
};

#pragma pack(pop)

mDNSDiscoveryService::mDNSDiscoveryService() = default;

mDNSDiscoveryService::~mDNSDiscoveryService() = default;

std::unordered_set<uint8_t>
mDNSDiscoveryService::find_modules(const std::chrono::duration<double> wait_time) {
    std::unordered_set<uint8_t> modules{};

    const socket_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("socket() failed: %s\n", strerror(errno));
        return modules;
    }

    constexpr int reuse = 1;
    timeval tv{};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
#ifdef _WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof tv);
    // Windows does not support SO_REUSEPORT
#else
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(MDNS_PORT);
    localAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, reinterpret_cast<sockaddr *>(&localAddr), sizeof(localAddr)) < 0) {
        printf("bind() failed: %s\n", strerror(errno));
        CLOSE_SOCKET(sock);
        return modules;
    }

    // Join mDNS multicast group
    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = inet_addr(MDNS_GROUP);
    mreq.imr_interface.s_addr = INADDR_ANY;

#ifdef _WIN32
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq, sizeof(mreq)) < 0) {
#else
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
#endif
        printf("setsockopt() failed: %s\n", strerror(errno));
        CLOSE_SOCKET(sock);
        return modules;
    }

    // Send mDNS query and get responses
    sockaddr_in mcastAddr{};
    mcastAddr.sin_family = AF_INET;
    mcastAddr.sin_port = htons(MDNS_PORT);
    inet_pton(AF_INET, MDNS_GROUP, &mcastAddr.sin_addr);
    const auto start = std::chrono::system_clock::now();
    std::vector<std::unique_ptr<std::vector<uint8_t>>> responses;

    while (std::chrono::system_clock::now() - start < wait_time) {
        send_mdns_query(sock, mcastAddr);

        std::this_thread::sleep_for(wait_time / 5);

        responses.emplace_back(std::make_unique<std::vector<uint8_t>>());
        responses.back()->resize(RECV_BLOCK_SIZE);
#ifdef _WIN32
        const auto len = recv(sock, (char *)responses.back()->data(), RECV_BLOCK_SIZE, 0);
#else
        const auto len = recv(sock, responses.back()->data(), RECV_BLOCK_SIZE, 0);
#endif
        if (len > 0) {
            responses.back()->resize(len);
        } else {
            responses.pop_back();
        }
    }

    CLOSE_SOCKET(sock);

    this->module_to_mdns.clear();

    for (const auto &response : responses) {
        if (const auto parsed_response = parse_response(response->data(), response->size());
            parsed_response.has_value()) {
            modules.insert(parsed_response.value().id);
            this->module_to_mdns.insert({parsed_response.value().id, parsed_response.value()});
        }
    }

    return modules;
}

std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>>
mDNSDiscoveryService::get_lossy_clients(
    const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
    std::vector<uint8_t> &skip_modules) {
    return this->create_clients<UDPClient>(rx_queue, skip_modules);
}

std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>>
mDNSDiscoveryService::get_lossless_clients(
    const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
    std::vector<uint8_t> &skip_modules) {
    return this->create_clients<TCPClient>(rx_queue, skip_modules);
}

template <typename T>
std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>>
mDNSDiscoveryService::create_clients(
    const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue,
    std::vector<uint8_t> &skip_modules) {
    std::unordered_map<uint8_t, std::shared_ptr<ICommunicationClient>> clients;

    for (const auto &[id, module] : this->module_to_mdns) {
        if (std::find(skip_modules.begin(), skip_modules.end(), id) != skip_modules.end()) {
            continue;
        }

        const auto client = std::make_shared<T>(module.ip, rx_queue);
        client->init();

        for (const auto &connected_module : module.connected_module_ids) {
            // todo: add only if not connected directly
            clients[connected_module] = client;
        }

        clients[id] = client;
    }

    return clients;
}

void mDNSDiscoveryService::send_mdns_query(const socket_t sock, const sockaddr_in &addr) {
    query_header header{};
    header.id = htons(0);
    header.flags = htons(0x0000);
    header.num_questions = htons(1);
    header.num_answers = htons(0);
    header.num_authority = htons(0);
    header.num_additional = htons(0);

    constexpr uint8_t domain_name[] = {
        13,  '_', 'r', 'o', 'b', 'o', 't', 'c', 'o', 'n', 't', 'r', 'o',
        'l', 4,   '_', 't', 'c', 'p', 5,   'l', 'o', 'c', 'a', 'l', 0,
    };

    query_footer footer;
    footer.type = htons(0x00FF);
    footer.class_id = htons(0x0001);

    uint8_t buffer[1024] = {};
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), &domain_name, sizeof(domain_name));
    memcpy(buffer + sizeof(header) + sizeof(domain_name), &footer, sizeof(footer));
#ifdef _WIN32
    sendto(sock, (char *)&buffer, sizeof(header) + sizeof(domain_name) + sizeof(footer), 0,
           (sockaddr *)&addr, sizeof(addr));
#else
    sendto(sock, &buffer, sizeof(header) + sizeof(domain_name) + sizeof(footer), 0,
           (sockaddr *)&addr, sizeof(addr));
#endif
}

std::optional<mDNSRobotModule> mDNSDiscoveryService::parse_response(uint8_t *buffer,
                                                                    const int size) {
    int ptr = 0;
    mDNSRobotModule response{};

    // Header
    if (size < sizeof(query_header)) {
        return std::nullopt;
    }
    const auto h = reinterpret_cast<query_header *>(buffer + ptr);
    ptr += sizeof(query_header);
    h->num_questions = ntohs(h->num_questions);
    h->num_answers = ntohs(h->num_answers);
    h->num_authority = ntohs(h->num_authority);
    h->num_additional = ntohs(h->num_additional);

    // Questions
    for (int i = 0; i < h->num_questions; i++) {
        if (ptr > size) {
            return std::nullopt;
        }

        // We ignore questions for now
        auto [name, new_ptr] = read_mdns_name(buffer, size, ptr);
        if (new_ptr < 1) {
            return std::nullopt;
        }
        ptr = new_ptr;
        ptr += sizeof(query_footer);
    }

    // Answers and authority (we do not care about authority).
    bool robot_module = false;
    for (int i = 0; i < h->num_answers + h->num_authority + h->num_additional; i++) {
        if (ptr > size) {
            return std::nullopt;
        }
        // We assume that the boards mdns does not send any questions asking for
        // other boards (and thus does not compress the domain name we are looking
        // for).

        const auto [name, new_ptr] = read_mdns_name(buffer, size, ptr);
        if (new_ptr < 1) {
            return std::nullopt;
        }
        ptr = new_ptr;

        robot_module |= name.find("_robotcontrol") != std::string::npos;
        response.hostname = name;

        const auto a = reinterpret_cast<answer *>(buffer + ptr);
        a->type = ntohs(a->type);
        a->answer_class = ntohs(a->answer_class);
        a->ttl = ntohs(a->ttl);
        a->data_length = ntohs(a->data_length);
        ptr += sizeof(answer);

        // A-Record
        if (a->type == 1 && robot_module) {
            std::vector<uint8_t> data;
            data.resize(a->data_length);
            std::memcpy(data.data(), buffer + ptr, a->data_length);

            std::stringstream ip;
            for (int j = 0; j < a->data_length; j++) {
                ip << static_cast<int>(data[j]);
                if (j < a->data_length - 1) {
                    ip << '.';
                }
            }
            response.ip = ip.str();
        }

        // TXT-Recrod
        if (a->type == 16 && robot_module) {
            int inner_ptr = ptr;
            while (inner_ptr < a->data_length + ptr) {
                const int len = buffer[inner_ptr++];
                std::string s(reinterpret_cast<char *>(buffer + inner_ptr), len);
                inner_ptr += len;

                const auto split_string = split(s, '=');
                if (split_string.size() != 2) {
                    continue;
                }

                if (split_string[0] == MODULE_ID_STR) {
                    response.id = stoi(split_string[1]);
                }

                if (split_string[0] == MODULE_TYPE_STR) {
                    response.module_type = static_cast<ModuleType>(stoi(split_string[1]));
                }

                if (split_string[0] == CONNECTED_MODULES_STR) {
                    for (const auto connected_modules = split(split_string[1], ',');
                         const auto &module_id : connected_modules) {
                        response.connected_module_ids.emplace_back(stoi(module_id));
                    }
                }
            }
        }

        ptr += a->data_length;
    }

    return robot_module && is_valid_ipv4(response.ip) ? std::optional{response} : std::nullopt;
}

std::tuple<std::string, int> mDNSDiscoveryService::read_mdns_name(const uint8_t *buffer,
                                                                  const int size, int ptr) {
    int len = 0;
    std::stringstream ss;

    int i = 0;
    while (ptr < size) {
        if (0 >= len) {
            if (0 == buffer[ptr]) { // end
                ptr++;
                break;
            }

            if (0 != i) {
                ss << ".";
            }

            if (buffer[ptr] >= 0xC0) { // compressed
                ptr++;
                if (buffer[ptr] < 0 || buffer[ptr] > ptr) {
                    return {"", -1};
                }
                const auto [name, l] = read_mdns_name(buffer, size, buffer[ptr]);
                if (l < 1) {
                    return {"", -1};
                }
                ptr++;
                ss << name;
                break;
            }

            len = buffer[ptr]; // update length
        } else {
            len--;
            ss << buffer[ptr];
        }
        ptr++;

        i++;
    }

    return {ss.str(), ptr};
}
