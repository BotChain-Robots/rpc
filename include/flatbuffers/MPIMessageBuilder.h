//
// Created by Johnathon Slightham on 2025-06-30.
//

#ifndef MPIMESSAGEBUILDER_H
#define MPIMESSAGEBUILDER_H

#include <string>
#include <vector>

#include "../flatbuffers_generated/MPIMessage_generated.h"
#include "SerializedMessage.h"
#include "flatbuffers/flatbuffers.h"

namespace Flatbuffers {
class MPIMessageBuilder {
  public:
    MPIMessageBuilder() : builder_(1024) {
    }

    SerializedMessage build_mpi_message(Messaging::MessageType type, uint8_t sender,
                                        uint8_t destination, uint16_t sequence_number,
                                        bool is_durable, uint8_t tag,
                                        const std::vector<uint8_t> &payload);

    static const Messaging::MPIMessage *parse_mpi_message(const uint8_t *buffer);

  private:
    flatbuffers::FlatBufferBuilder builder_;
};
} // namespace Flatbuffers

#endif //MPIMESSAGEBUILDER_H
