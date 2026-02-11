//
// Created by Johnathon Slightham on 2025-07-05.
//

#ifndef SERIALIZEDMESSAGE_H
#define SERIALIZEDMESSAGE_H

namespace Flatbuffers {
struct SerializedMessage {
    void *data;
    size_t size;
};
} // namespace Flatbuffers

#endif // SERIALIZEDMESSAGE_H
