
#ifndef CALLBUILDER_H
#define CALLBUILDER_H

#include <vector>

#include "SerializedMessage.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers_generated/ReturnCall_generated.h"
#include "flatbuffers_generated/SendCall_generated.h"

namespace Flatbuffers {

class CallBuilder {
  public:
    CallBuilder() : builder_(1024) {
    }

    SerializedMessage build_send_call(uint8_t tag, uint8_t unique_id,
                                      const std::vector<uint8_t> &parameters);

    static const Messaging::ReturnCall *parse_return_call(const uint8_t *buffer);

  private:
    flatbuffers::FlatBufferBuilder builder_;
};
} // namespace Flatbuffers

#endif // CALLBUILDER_H
