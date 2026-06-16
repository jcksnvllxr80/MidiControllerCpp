#include "mc/adapters/mcu/McpExpander.h"

namespace mc::mcu {

namespace {
// MCP23017 register addresses (IOCON.BANK = 0, the power-on default; same map the
// Python MCP23017_R1 class used).
constexpr uint8_t IODIRA   = 0x00;
constexpr uint8_t IODIRB   = 0x01;
constexpr uint8_t IPOLA    = 0x02;
constexpr uint8_t IPOLB    = 0x03;
constexpr uint8_t GPINTENA = 0x04;
constexpr uint8_t GPINTENB = 0x05;
constexpr uint8_t INTCONA  = 0x08;
constexpr uint8_t INTCONB  = 0x09;
constexpr uint8_t IOCON    = 0x0A;
constexpr uint8_t GPPUA    = 0x0C;
constexpr uint8_t GPPUB    = 0x0D;
constexpr uint8_t INTFA    = 0x0E;
constexpr uint8_t GPIOA    = 0x12;

// Footswitch/selector bit masks, derived from the Python pin constants:
//   Bank A: FSW1=A0, FSW2=A1, FSW3=A2                       -> 0x07
//   Bank B: FSW4=B0, FSW5=B2, Selector=B7 (rotary push)     -> 0x85
constexpr uint8_t MASK_A = 0x07;
constexpr uint8_t MASK_B = 0x85;
constexpr uint8_t SEL_B  = 0x80;  // selector bit within bank B (B7)
}  // namespace

void McpExpander::w8(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(i2c_, addr_, buf, 2, false);
}

uint8_t McpExpander::r8(uint8_t reg) {
    uint8_t v = 0;
    i2c_write_blocking(i2c_, addr_, &reg, 1, true);  // repeated start
    i2c_read_blocking(i2c_, addr_, &v, 1, false);
    return v;
}

void McpExpander::begin() {
    // Functionally identical to the Pi's Footswitches.py setup, minus its dead
    // writes (it set DEFVAL for every pin, but DEFVAL is only consulted when
    // INTCON=1; here INTCON=0 — interrupt-on-change compares to the previous value —
    // so DEFVAL is never read and we skip it):
    //   IODIR  = switch pins as inputs
    //   GPPU   = pull-ups on, except the selector (B7) which the Pi left floating
    //   IPOL   = invert the selector (B7) so its bit reads pressed-high
    //   GPINTEN= interrupt-on-change enabled for every switch
    //   INTCON = 0 (compare to previous value)
    //   IOCON  = 0x02 (INTPOL=1: INT outputs active-high)
    w8(IODIRA, MASK_A);   w8(IODIRB, MASK_B);
    w8(IPOLA, 0x00);      w8(IPOLB, SEL_B);
    w8(GPINTENA, MASK_A); w8(GPINTENB, MASK_B);
    w8(INTCONA, 0x00);    w8(INTCONB, 0x00);
    w8(IOCON, 0x02);
    w8(GPPUA, MASK_A);    w8(GPPUB, MASK_B & ~SEL_B);  // 0x05: selector pull-up off
    readGpio();  // clear any power-on interrupt latch
}

uint16_t McpExpander::readGpio() {
    // GPIOA then GPIOB (auto-increment in BANK=0): low byte = A, high byte = B.
    uint8_t reg = GPIOA;
    uint8_t v[2] = {0, 0};
    i2c_write_blocking(i2c_, addr_, &reg, 1, true);
    i2c_read_blocking(i2c_, addr_, v, 2, false);
    return static_cast<uint16_t>(v[0] | (v[1] << 8));
}

uint16_t McpExpander::readIntf() {
    uint8_t reg = INTFA;
    uint8_t v[2] = {0, 0};
    i2c_write_blocking(i2c_, addr_, &reg, 1, true);
    i2c_read_blocking(i2c_, addr_, v, 2, false);
    return static_cast<uint16_t>(v[0] | (v[1] << 8));
}

}  // namespace mc::mcu
