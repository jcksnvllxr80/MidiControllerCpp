#pragma once
//
// McuInput — IInput from real hardware, wired the way the (unchanged) PCB is:
//   * 5 footswitches + the rotary push button come from an MCP23017 expander over
//     I2C (McpExpander), exactly as on the Pi. They are debounced and classified
//     short/long via the domain ButtonSM; the push button's hold time becomes a
//     RotaryPress (seconds).
//   * the quadrature rotary encoder A/B are native Pico GPIO (CW/CCW per detent).
//
// Expander reads are triggered by the MCP INT lines: the expander raises INT
// (active-high, IOCON INTPOL=1) on any switch change and holds it until we read
// GPIO, so we read over I2C only when an INT line is high — exactly the Pi's model.
// poll() is non-blocking: it services inputs and returns the next queued event, or
// false if none.
//
#include <array>
#include <cstdint>
#include <vector>

#include "mc/adapters/mcu/McpExpander.h"
#include "mc/domain/ButtonSM.h"
#include "mc/ports/IClock.h"
#include "mc/ports/IInput.h"

namespace mc::mcu {

class McuInput : public IInput {
public:
    // fswBits: expander bit index per footswitch (button order 1..N).
    // selectorBit: expander bit of the rotary push button (15 on this board).
    // intA/intB: Pico GPIOs wired to the MCP INTA/INTB outputs.
    McuInput(const IClock& clock, McpExpander& exp, const uint8_t* fswBits, int fswCount,
             uint8_t selectorBit, uint8_t intA, uint8_t intB, uint8_t encoderA, uint8_t encoderB);
    void begin();  // configure encoder + INT GPIOs (the expander is begun separately)
    bool poll(InputEvent& out) override;

private:
    void service();
    void serviceExpander(double nowS);
    void push(const InputEvent& e);
    bool accept(int idx, bool level, double nowS);  // simple time debounce

    static constexpr double kDebounce = 0.005;        // 5 ms
    static constexpr int kStepsPerDetent = 4;         // typical mechanical encoder
    static constexpr size_t kRing = 32;

    const IClock& clock_;
    McpExpander& exp_;
    const uint8_t* fswBits_;
    int fswCount_;
    uint8_t selectorBit_;
    uint8_t intA_, intB_;
    uint8_t encA_, encB_;

    std::vector<ButtonSM> buttons_;  // one per footswitch

    // debounce: index 0..fswCount-1 = footswitches, fswCount = selector/rotary PB
    std::array<bool, 8> level_{};
    std::array<double, 8> changedAt_{};

    uint8_t encPrev_ = 0;
    int encAccum_ = 0;

    bool rotaryDown_ = false;
    double rotaryStart_ = 0.0;

    std::array<InputEvent, kRing> ring_{};
    size_t head_ = 0, tail_ = 0;
};

}  // namespace mc::mcu
