//
// Created by Johnathon Slightham on 2025-06-30.
//

#include "flatbuffers/MPIMessageBuilder.h"
#include "flatbuffers/SerializedMessage.h"

namespace Flatbuffers {
SerializedMessage MPIMessageBuilder::build_mpi_message(const Messaging::MessageType type,
                                                       const uint8_t sender,
                                                       const uint8_t destination,
                                                       const uint16_t sequence_number,
                                                       const bool is_durable, const uint8_t tag,
                                                       const std::vector<uint8_t> &payload) {
    builder_.Clear();

    const auto payload_vector = builder_.CreateVector(payload);

    const auto message = Messaging::CreateMPIMessage(
        builder_, type, sender, destination, sequence_number, is_durable,
        static_cast<int>(payload.size()), tag, payload_vector);

    builder_.Finish(message);

    return {builder_.GetBufferPointer(), builder_.GetSize()};
}

const Messaging::MPIMessage *MPIMessageBuilder::parse_mpi_message(const uint8_t *buffer) {
    return flatbuffers::GetRoot<Messaging::MPIMessage>(buffer);
}
} // namespace Flatbuffers
