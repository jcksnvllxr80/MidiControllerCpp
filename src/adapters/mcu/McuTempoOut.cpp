#include "mc/adapters/mcu/McuTempoOut.h"

#include "hardware/gpio.h"

namespace mc::mcu {

McuTempoOut::McuTempoOut(const uint8_t* pins, int count) : pins_(pins), count_(count > 4 ? 4 : count) {}

void McuTempoOut::begin() {
    for (int i = 0; i < count_; ++i) {
        gpio_init(pins_[i]);
        gpio_set_dir(pins_[i], GPIO_OUT);
        gpio_put(pins_[i], 0);
    }
}

void McuTempoOut::writeAll(bool high) {
    for (int i = 0; i < count_; ++i) gpio_put(pins_[i], high);
}

bool McuTempoOut::onTimer(repeating_timer_t* t) {
    auto* self = static_cast<McuTempoOut*>(t->user_data);
    self->phase_ = !self->phase_;
    self->writeAll(self->phase_);
    return true;  // keep repeating
}

void McuTempoOut::setBpm(double bpm) {
    if (running_) {
        cancel_repeating_timer(&timer_);
        running_ = false;
    }
    if (bpm <= 0.0) {
        writeAll(false);
        return;
    }
    // Toggle every half-period so a full cycle == one beat.
    int64_t halfPeriodUs = static_cast<int64_t>(60.0e6 / bpm / 2.0);
    phase_ = false;
    writeAll(false);
    // Negative delay = fire on a strict period regardless of callback duration.
    add_repeating_timer_us(-halfPeriodUs, &McuTempoOut::onTimer, this, &timer_);
    running_ = true;
}

void McuTempoOut::tap() {
    // Single manual pulse (~5 ms). tap() is user-initiated, so a brief block is fine.
    writeAll(true);
    sleep_ms(5);
    writeAll(false);
}

}  // namespace mc::mcu
