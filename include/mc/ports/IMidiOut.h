#pragma once
//
// IMidiOut — the MIDI output port. The domain emits fully-formed MidiMessages;
// adapters decide what to do with the bytes (log them in the sim, push them out
// a DIN/USB jack on the microcontroller). Channel->status math already happened
// in MidiMessage, so this port is a dumb sink — exactly what makes the output
// testable with a mock.
//
#include "mc/domain/MidiMessage.h"

namespace mc {

class IMidiOut {
public:
    virtual ~IMidiOut() = default;
    virtual void send(const MidiMessage& msg) = 0;
};

}  // namespace mc
