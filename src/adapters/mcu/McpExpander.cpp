#include "mc/adapters/mcu/McpExpander.h"

#include "mc/adapters/mcu/Log.h"

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

// Per-transfer timeout so a stuck/unpowered bus (e.g. a wiring or level-shift
// fault) degrades to "no expander" instead of hanging the whole boot. ~2 ms is
// far longer than any real 2-byte transfer at 400 kHz but still imperceptible.
namespace {
constexpr uint kI2cTimeoutUs = 2000;
}  // namespace

void McpExpander::w8(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    int rc = i2c_write_timeout_us(i2c_, addr_, buf, 2, false, kI2cTimeoutUs);
    if (rc < 0) LOG_E("mcp", "w8 reg=0x%02X val=0x%02X failed rc=%d", reg, val, rc);
}

uint8_t McpExpander::r8(uint8_t reg) {
    uint8_t v = 0;
    int rc = i2c_write_timeout_us(i2c_, addr_, &reg, 1, true, kI2cTimeoutUs);  // repeated start
    if (rc < 0) { LOG_E("mcp", "r8 reg=0x%02X write failed rc=%d", reg, rc); return v; }
    rc = i2c_read_timeout_us(i2c_, addr_, &v, 1, false, kI2cTimeoutUs);
    if (rc < 0) LOG_E("mcp", "r8 reg=0x%02X read failed rc=%d", reg, rc);
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
    //   INTCON = 0 (compare to previous value — fires on BOTH press AND release)
    //   IOCON  = 0x02 (INTPOL=1: INT outputs active-high, push-pull)
    //   MIRROR is intentionally NOT set: INTA and INTB are separate dedicated wires
    //   to the Pico. Both are wired, both are armed with edge-rise IRQs in McuInput.
    w8(IODIRA, MASK_A);   w8(IODIRB, MASK_B);
    w8(IPOLA, 0x00);      w8(IPOLB, 0x00);  // selector not inverted — it's active-high (connects to VCC)
    w8(GPINTENA, MASK_A); w8(GPINTENB, MASK_B);
    w8(INTCONA, 0x00);    w8(INTCONB, 0x00);
    w8(IOCON, 0x02);
    w8(GPPUA, MASK_A);    w8(GPPUB, MASK_B & ~SEL_B);  // 0x05: selector pull-up off (active-high, floats low when released)
    readGpio();  // clear any power-on interrupt latch
    LOG_I("mcp", "MCP23017 @ 0x%02X ready (IOC both-edges, INT active-high)", addr_);
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
