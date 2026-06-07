#pragma once
// A fake IMidiOut that records every message, for exact-sequence assertions.
#include <string>
#include <vector>

#include "mc/ports/IMidiOut.h"

namespace mc::test {

class RecordingMidiOut : public IMidiOut {
public:
    void send(const MidiMessage& msg) override { messages.push_back(msg); }
    void clear() { messages.clear(); }

    // Flattened byte view, handy for compact assertions.
    std::vector<std::vector<uint8_t>> byteSeq() const {
        std::vector<std::vector<uint8_t>> out;
        for (const auto& m : messages) out.push_back(m.bytes());
        return out;
    }
    std::string dump() const {
        std::string s;
        for (const auto& m : messages) { s += m.toString(); s += '\n'; }
        return s;
    }

    std::vector<MidiMessage> messages;
};

}  // namespace mc::test
