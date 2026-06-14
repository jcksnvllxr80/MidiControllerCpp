#include "mc/adapters/mcu/McuInput.h"

#include <string>

#include "hardware/gpio.h"

namespace mc::mcu {

namespace {
// Quadrature delta table: index = (prevAB << 2) | curAB -> -1 / 0 / +1.
constexpr int8_t kQuad[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
}  // namespace

McuInput::McuInput(const IClock& clock, const uint8_t* footswitches, int footswitchCount, uint8_t encoderA,
                   uint8_t encoderB, uint8_t rotaryPb)
    : clock_(clock),
      fsw_(footswitches),
      fswCount_(footswitchCount > 6 ? 6 : footswitchCount),
      encA_(encoderA),
      encB_(encoderB),
      rotaryPb_(rotaryPb) {
    buttons_.reserve(fswCount_);
    for (int i = 0; i < fswCount_; ++i) {
        buttons_.emplace_back(std::to_string(i + 1), "long", clock_);
    }
}

void McuInput::begin() {
    auto inPullUp = [](uint8_t pin) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    };
    for (int i = 0; i < fswCount_; ++i) {
        inPullUp(fsw_[i]);
        level_[i] = true;  // released (pulled high)
    }
    inPullUp(rotaryPb_);
    level_[fswCount_] = true;
    inPullUp(encA_);
    inPullUp(encB_);
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

void McuInput::service() {
    const double now = clock_.now();

    // Footswitches (active-low). false == pressed, matching ButtonSM::onEdge.
    for (int i = 0; i < fswCount_; ++i) {
        bool lvl = gpio_get(fsw_[i]) != 0;
        if (accept(i, lvl, now)) {
            ButtonSM::Result r = buttons_[i].onEdge(lvl);
            if (r.kind == ButtonSM::PressKind::Short)
                push({InputEvent::Type::FootswitchShort, i + 1, 0});
            else if (r.kind == ButtonSM::PressKind::Long)
                push({InputEvent::Type::FootswitchLong, i + 1, 0});
        }
    }

    // Rotary push button (active-low): time the hold, emit on release.
    {
        bool lvl = gpio_get(rotaryPb_) != 0;
        if (accept(fswCount_, lvl, now)) {
            if (!lvl) {  // pressed
                rotaryDown_ = true;
                rotaryStart_ = now;
            } else if (rotaryDown_) {  // released
                rotaryDown_ = false;
                push({InputEvent::Type::RotaryPress, 0, now - rotaryStart_});
            }
        }
    }

    // Encoder quadrature.
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
