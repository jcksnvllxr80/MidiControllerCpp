#pragma once
//
// McuMidiOut — IMidiOut over a hardware UART at the MIDI rate (31250 baud, 8N1).
// One instance per DIN jack. TeeMidiOut mirrors to both jacks; for per-channel
// routing wrap your own IMidiOut that dispatches on msg.channel().
//
#include "hardware/uart.h"

#include "mc/ports/IMidiOut.h"

namespace mc::mcu {

class McuMidiOut : public IMidiOut {
public:
    McuMidiOut(uart_inst_t* uart, unsigned txPin);
    void begin();  // call once before use
    void send(const MidiMessage& msg) override;

private:
    uart_inst_t* uart_;
    unsigned txPin_;
};

// Mirror every message to two outputs (the two DIN jacks).
class TeeMidiOut : public IMidiOut {
public:
    TeeMidiOut(IMidiOut* a, IMidiOut* b) : a_(a), b_(b) {}
    void send(const MidiMessage& msg) override {
        if (a_) a_->send(msg);
        if (b_) b_->send(msg);
    }

private:
    IMidiOut* a_;
    IMidiOut* b_;
};

}  // namespace mc::mcu
