#include "flatbuffers/CallBuilder.h"
#include "flatbuffers/SerializedMessage.h"

namespace Flatbuffers {
SerializedMessage CallBuilder::build_send_call(uint8_t tag, uint8_t unique_id,
                                               const std::vector<uint8_t> &parameters) {
    builder_.Clear();

    const auto parameters_vector = builder_.CreateVector(parameters);

    const auto message = Messaging::CreateSendCall(
        builder_, tag, unique_id, static_cast<int>(parameters.size()), parameters_vector);

    builder_.Finish(message);

    return {builder_.GetBufferPointer(), builder_.GetSize()};
}

const Messaging::ReturnCall *CallBuilder::parse_return_call(const uint8_t *buffer) {
    return flatbuffers::GetRoot<Messaging::ReturnCall>(buffer);
}

} // namespace Flatbuffers
