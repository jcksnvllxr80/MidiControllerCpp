#pragma once
//
// McuMidiOut — IMidiOut over I2C to the PIC18F26K80 MIDI bridge at address 0x04,
// exactly as the Raspberry Pi firmware did (MIDI.py: writeRaw8 each byte to 0x04).
// The PIC fans every byte out to all 6 physical jacks (2x DIN-5 + 4x TRS-MIDI), so
// the Pico does NOT generate MIDI on a UART — it just feeds the bridge.
//
// The I2C bus is shared with the MCP23017 expander and is brought up once in main;
// this class only does the per-message writes.
//
#include <cstdint>

#include "hardware/i2c.h"

#include "mc/ports/IMidiOut.h"

namespace mc::mcu {

class McuMidiOut : public IMidiOut {
public:
    McuMidiOut(i2c_inst_t* i2c, uint8_t addr) : i2c_(i2c), addr_(addr) {}
    void send(const MidiMessage& msg) override;

private:
    i2c_inst_t* i2c_;
    uint8_t addr_;
};

}  // namespace mc::mcu
