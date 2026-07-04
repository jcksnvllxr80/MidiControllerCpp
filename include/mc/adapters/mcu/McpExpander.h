#pragma once
//
// McpExpander — MCP23017 16-bit I2C GPIO expander, configured BYTE-FOR-BYTE the
// way the Raspberry Pi firmware configured it (Footswitches.py / MCP23017_R1.py).
// On the (unchanged) PCB this expander carries the 5 footswitches and the rotary
// encoder's push button; the Pico reads them over the shared I2C bus exactly as
// the Pi did, so nothing about the board's wiring or protocol changes.
//
// Bit layout on the expander (matches the Python pin constants):
//   bit 0  (A0) = Footswitch 1      bit 8  (B0) = Footswitch 4
//   bit 1  (A1) = Footswitch 2      bit 10 (B2) = Footswitch 5
//   bit 2  (A2) = Footswitch 3      bit 15 (B7) = Selector / rotary push button
//
// The I2C bus itself (i2c_init + SDA/SCL function/pull-ups) is brought up once in
// main — it is shared with the PIC MIDI bridge at 0x04 — so begin() here only
// writes this device's registers.
//
#include <cstdint>

#include "hardware/i2c.h"

namespace mc::mcu {

class McpExpander {
public:
    McpExpander(i2c_inst_t* i2c, uint8_t addr) : i2c_(i2c), addr_(addr) {}

    // Write the exact register set the Pi wrote. See the .cpp for the derivation
    // of every value from the Python setup loop.
    void begin();

    // Current input state as a 16-bit word: bit n == expander pin n (B<<8 | A).
    // Reading GPIO also clears any pending interrupt-on-change condition.
    uint16_t readGpio();

    // Interrupt-flag register (INTFB<<8 | INTFA): which pins triggered an INT.
    uint16_t readIntf();

private:
    void w8(uint8_t reg, uint8_t val);
    uint8_t r8(uint8_t reg);

    i2c_inst_t* i2c_;
    uint8_t addr_;
};

}  // namespace mc::mcu
