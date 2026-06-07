#pragma once
//
// MidiMessage — exact MIDI byte construction. The Python brain (MIDI.py) built
// raw bytes and handed them to the Arduino over I2C; the channel->status-byte
// math lived there. We keep that math here, in the testable domain, so output
// is verifiable without any hardware.
//
//   Control Change: [0xB0 | (ch-1), cc, value]    (3 bytes)
//   Program Change: [0xC0 | (ch-1), program]       (2 bytes)
//
// Channel is 1..16 (matches the YAML/JSON config and MIDI.py's cc_dict keys).
//
#include <cstdint>
#include <string>
#include <vector>

namespace mc {

class MidiMessage {
public:
    enum class Type { ControlChange, ProgramChange };

    static constexpr uint8_t kCcStatusBase = 0xB0;  // MIDI.py cc_dict: ch1 -> 0xB0
    static constexpr uint8_t kPcStatusBase = 0xC0;  // MIDI.py pc_dict: ch1 -> 0xC0

    // change_num == cc number, value == data byte.
    static MidiMessage controlChange(int channel, int cc, int value);
    // program == program number.
    static MidiMessage programChange(int channel, int program);

    Type type() const { return type_; }
    int channel() const { return channel_; }
    const std::vector<uint8_t>& bytes() const { return bytes_; }

    // e.g. "CC  ch=2  cc=102 val=127  [B1 66 7F]"
    std::string toString() const;

    bool operator==(const MidiMessage& o) const { return type_ == o.type_ && channel_ == o.channel_ && bytes_ == o.bytes_; }
    bool operator!=(const MidiMessage& o) const { return !(*this == o); }

private:
    MidiMessage() = default;
    Type type_{};
    int channel_{};
    std::vector<uint8_t> bytes_;
};

}  // namespace mc
