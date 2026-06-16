#include "mc/adapters/mcu/McuInput.h"

#include <string>

#include "hardware/gpio.h"

namespace mc::mcu {

namespace {
// Quadrature delta table: index = (prevAB << 2) | curAB -> -1 / 0 / +1.
constexpr int8_t kQuad[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
}  // namespace

McuInput::McuInput(const IClock& clock, McpExpander& exp, const uint8_t* fswBits, int fswCount,
                   uint8_t selectorBit, uint8_t intA, uint8_t intB, uint8_t encoderA, uint8_t encoderB)
    : clock_(clock),
      exp_(exp),
      fswBits_(fswBits),
      fswCount_(fswCount > 6 ? 6 : fswCount),
      selectorBit_(selectorBit),
      intA_(intA),
      intB_(intB),
      encA_(encoderA),
      encB_(encoderB) {
    buttons_.reserve(fswCount_);
    for (int i = 0; i < fswCount_; ++i) {
        buttons_.emplace_back(std::to_string(i + 1), "long", clock_);
    }
}

void McuInput::begin() {
    // Encoder A/B: native inputs with pull-ups (idle high, pull low).
    for (uint8_t pin : {encA_, encB_}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }
    // MCP INT lines: active-high push-pull (IOCON INTPOL=1, ODR=0), idle low — a
    // pull-down keeps them defined if the trace floats before the expander drives.
    for (uint8_t pin : {intA_, intB_}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_down(pin);
    }

    // All switches read released at boot (footswitches pulled high; the selector is
    // inverted via IPOL, so "released" is handled in serviceExpander()).
    for (auto& l : level_) l = true;

    encPrev_ = static_cast<uint8_t>((gpio_get(encA_) << 1) | gpio_get(encB_));
    double now = clock_.now();
    for (auto& t : changedAt_) t = now;
}

void McuInput::push(const InputEvent& e) {
    ring_[head_] = e;
    head_ = (head_ + 1) % kRing;
    if (head_ == tail_) tail_ = (tail_ + 1) % kRing;  // overwrite oldest if full
}

bool McuInput::accept(int idx, bool level, double nowS) {
    if (level == level_[idx]) return false;
    if (nowS - changedAt_[idx] < kDebounce) return false;
    level_[idx] = level;
    changedAt_[idx] = nowS;
    return true;
}

void McuInput::serviceExpander(double nowS) {
    // The MCP holds INT high until GPIO is read, so read only when a line is
    // asserted; the single 16-bit read covers both banks and clears the interrupt.
    if (!gpio_get(intA_) && !gpio_get(intB_)) return;

    const uint16_t word = exp_.readGpio();

    // Footswitches (active-low: bit high == released, low == pressed).
    for (int i = 0; i < fswCount_; ++i) {
        bool released = (word >> fswBits_[i]) & 1u;
        if (accept(i, released, nowS)) {
            ButtonSM::Result r = buttons_[i].onEdge(released);
            if (r.kind == ButtonSM::PressKind::Short)
                push({InputEvent::Type::FootswitchShort, i + 1, 0});
            else if (r.kind == ButtonSM::PressKind::Long)
                push({InputEvent::Type::FootswitchLong, i + 1, 0});
        }
    }

    // Selector / rotary push button. Its IPOL bit is set on the expander, so the
    // reported bit is inverted vs the footswitches: bit high == pressed. We map it
    // back to the same "released" convention and time the hold -> RotaryPress.
    bool released = ((word >> selectorBit_) & 1u) == 0;
    if (accept(fswCount_, released, nowS)) {
        if (!released) {  // pressed
            rotaryDown_ = true;
            rotaryStart_ = nowS;
        } else if (rotaryDown_) {  // released
            rotaryDown_ = false;
            push({InputEvent::Type::RotaryPress, 0, nowS - rotaryStart_});
        }
    }
}

void McuInput::service() {
    const double now = clock_.now();

    serviceExpander(now);

    // Encoder quadrature (native GPIO, read every pass for responsiveness).
    uint8_t cur = static_cast<uint8_t>((gpio_get(encA_) << 1) | gpio_get(encB_));
    if (cur != encPrev_) {
        encAccum_ += kQuad[(encPrev_ << 2) | cur];
        encPrev_ = cur;
        while (encAccum_ >= kStepsPerDetent) {
            encAccum_ -= kStepsPerDetent;
            push({InputEvent::Type::EncoderCW, 0, 0});
        }
        while (encAccum_ <= -kStepsPerDetent) {
            encAccum_ += kStepsPerDetent;
            push({InputEvent::Type::EncoderCCW, 0, 0});
        }
    }
}

bool McuInput::poll(InputEvent& out) {
    if (head_ == tail_) service();
    if (head_ == tail_) return false;
    out = ring_[tail_];
    tail_ = (tail_ + 1) % kRing;
    return true;
}

}  // namespace mc::mcu
