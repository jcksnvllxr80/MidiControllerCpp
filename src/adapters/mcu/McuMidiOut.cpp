#include "mc/adapters/mcu/McuMidiOut.h"

#include <vector>

#include "pico/stdlib.h"  // sleep_us

namespace mc::mcu {

namespace {
// One MIDI byte at 31250 baud, 8N1 = 10 bits = 320 us. The PIC bridge re-clocks
// each I2C byte out its UART at this rate, buffered by only an 8-byte FIFO with no
// I2C clock stretching (i2c1.c: SSP1CON1=0x36, CKP held released; EUSART1_Write
// spin-waits on a full buffer). So we must not hand it bytes faster than the UART
// drains, or the FIFO overruns and MIDI is dropped — the Pico's hardware I2C is far
// faster than the Pi's Python/smbus, where this pacing came "for free".
constexpr unsigned kMidiByteUs = 320;
}  // namespace

void McuMidiOut::send(const MidiMessage& msg) {
    // Stream the (2- or 3-byte) message in one I2C transaction — the PIC's ISR
    // forwards every data byte — then hold off the next message until this one has
    // clocked out of the UART, so the 8-byte FIFO never overruns.
    const std::vector<uint8_t>& b = msg.bytes();
    if (b.empty()) return;
    i2c_write_blocking(i2c_, addr_, b.data(), b.size(), false);
    sleep_us(kMidiByteUs * static_cast<unsigned>(b.size()));
}

}  // namespace mc::mcu
