#pragma once
//
// McuInput — IInput from real hardware, wired the way the (unchanged) PCB is:
//   * 5 footswitches + the rotary push button come from an MCP23017 expander over
//     I2C (McpExpander), exactly as on the Pi. They are debounced and classified
//     short/long via the domain ButtonSM; the push button's hold time becomes a
//     RotaryPress (seconds).
//   * the quadrature rotary encoder A/B are native Pico GPIO, read on both-edge
//     interrupts (like the Pi), per-line debounced, and decoded with a port of the
//     original RotaryEncoder.py get_rotary_movement logic.
//
// Expander reads are triggered by the MCP INT lines: the expander raises INT
// (active-high, IOCON INTPOL=1) on any switch change and holds it until we read
// GPIO, so we read over I2C only when an INT line is high — exactly the Pi's model.
// poll() is non-blocking: it services inputs and returns the next queued event, or
// false if none.
//
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "hardware/gpio.h"  // gpio_get (diagnostic accessor)
#include "pico/time.h"      // repeating_timer_t / time_us_32

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

    // DIAGNOSTIC: invoked on every raw encoder-pin level change, before any
    // quadrature decoding. Wire it to the activity LED to prove whether the
    // GP14/GP15 lines actually move when the knob is turned.
    void setEncoderEdgeDebug(std::function<void()> fn) { encoderEdgeDebug_ = std::move(fn); }

    // DIAGNOSTIC live state for the on-screen probe.
    uint8_t dbgEncAB() const {
        return static_cast<uint8_t>((gpio_get(encA_) << 1) | gpio_get(encB_));
    }
    uint32_t dbgEncEdges() const { return encEdges_; }
    int32_t dbgEncSteps() const { return encStepsTotal_; }
    uint32_t dbgEncA() const { return encEdgesA_; }  // debounced edges on line A
    uint32_t dbgEncB() const { return encEdgesB_; }  // debounced edges on line B

private:
    void service();
    void serviceExpander(double nowS);
    void push(const InputEvent& e);
    bool accept(int idx, bool level, double nowS);  // simple time debounce
    void decodeEncoder();          // decode one encoder edge (IRQ context)
    void serviceMcpInt();          // set intPending_ flag from IRQ context
    static void gpioIrqHandler();  // raw GPIO IRQ: dispatches encoder vs MCP INT
    static McuInput* s_isr_self_;

    static constexpr double kDebounce = 0.005;        // 5 ms
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

    // Encoder rest state is A=1,B=1 (seq=3). CW: seq visits 2 (A falls, B=1).
    // CCW: seq visits 1 (B falls, A=1). 2ms per-line debounce lets through only
    // the first departure per physical detent (~11 raw edges land within 2ms).
    static constexpr uint32_t kEncBounceUs = 2000;
    static constexpr uint32_t kEncCooldownUs = 333'000;  // 333 ms minimum between counted steps
    static constexpr double kSelBounceS = 0.030;    // 30 ms selector debounce (no HW caps)
    int encAStable_ = 1, encBStable_ = 1;           // debounced A/B levels
    uint32_t encALastUs_ = 0, encBLastUs_ = 0;      // last accepted edge time per line
    uint32_t encLastStepUs_ = 0;                    // time of last counted step (IRQ only)
    volatile int32_t encDelta_ = 0;    // net detents pending; IRQ writes, main drains
    volatile int32_t encStepsTotal_ = 0;  // DIAGNOSTIC: cumulative decoded steps
    volatile uint32_t encEdges_ = 0;   // raw edge count (for the optional LED probe)
    uint32_t lastEdges_ = 0;           // last drained edge count (main only)
    volatile uint32_t encEdgesA_ = 0;  // DIAGNOSTIC: debounced edges per line
    volatile uint32_t encEdgesB_ = 0;

    // Per-edge ring: IRQ pushes one entry per debounced A/B transition so service()
    // can log the sequence without calling printf in IRQ context.
    struct EncEdge {
        int8_t a, b;   // debounced GPIO levels at this edge
        int8_t seq;    // = a + 2*b  (0-3)
        int8_t move;   // +1=CW, -1=CCW, 0=rest/idle
    };
    static constexpr uint8_t kEncEdgeRing = 16;  // power-of-2 so uint8_t overflow is safe
    EncEdge encEdgeRing_[kEncEdgeRing]{};
    volatile uint8_t encEdgeHead_ = 0;  // written by IRQ
    uint8_t encEdgeTail_ = 0;           // read by service() only

    std::function<void()> encoderEdgeDebug_;  // optional raw-edge probe

    volatile bool intPending_ = false;  // set by IRQ when INTA or INTB rises; cleared by serviceExpander

    bool rotaryDown_ = false;
    double rotaryStart_ = 0.0;
    double selRecheckAt_ = 0.0;  // scheduled re-read time when selector change was debounced

    std::array<InputEvent, kRing> ring_{};
    size_t head_ = 0, tail_ = 0;
};

}  // namespace mc::mcu
