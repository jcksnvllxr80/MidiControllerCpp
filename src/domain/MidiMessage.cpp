#include "mc/domain/MidiMessage.h"

#include <array>
#include <cstdio>
#include <stdexcept>

namespace mc {

namespace {
uint8_t statusByte(uint8_t base, int channel) {
    if (channel < 1 || channel > 16) {
        throw std::out_of_range("MIDI channel must be 1..16, got " + std::to_string(channel));
    }
    return static_cast<uint8_t>(base | (channel - 1));
}
}  // namespace

MidiMessage MidiMessage::controlChange(int channel, int cc, int value) {
    MidiMessage m;
    m.type_ = Type::ControlChange;
    m.channel_ = channel;
    m.bytes_ = {statusByte(kCcStatusBase, channel), static_cast<uint8_t>(cc), static_cast<uint8_t>(value)};
    return m;
}

MidiMessage MidiMessage::programChange(int channel, int program) {
    MidiMessage m;
    m.type_ = Type::ProgramChange;
    m.channel_ = channel;
    m.bytes_ = {statusByte(kPcStatusBase, channel), static_cast<uint8_t>(program)};
    return m;
}

std::string MidiMessage::toString() const {
    char head[32];
    if (type_ == Type::ControlChange) {
        std::snprintf(head, sizeof head, "CC  ch=%-2d cc=%-3d val=%-3d ", channel_, bytes_[1], bytes_[2]);
    } else {
        std::snprintf(head, sizeof head, "PC  ch=%-2d prog=%-3d        ", channel_, bytes_[1]);
    }
    std::string out(head);
    out += " [";
    for (size_t i = 0; i < bytes_.size(); ++i) {
        char hex[4];
        std::snprintf(hex, sizeof hex, "%02X", bytes_[i]);
        if (i) out += ' ';
        out += hex;
    }
    out += ']';
    return out;
}

}  // namespace mc
