#pragma once
//
// McuTempoOut — ITempoOut driving up to 4 GPIO tempo jacks. Emits a square wave
// at the beat rate (bpm/60 Hz) via a repeating timer (one rising edge per beat).
// If your downstream device wants a short pulse instead of 50% duty, narrow the
// high phase in the timer callback.
//
#include <cstdint>

#include "pico/time.h"

#include "mc/ports/ITempoOut.h"

namespace mc::mcu {

class McuTempoOut : public ITempoOut {
public:
    McuTempoOut(const uint8_t* pins, int count);
    void begin();  // configure the GPIOs
    void setBpm(double bpm) override;
    void tap() override;

private:
    static bool onTimer(repeating_timer_t* t);
    void writeAll(bool high);

    const uint8_t* pins_;
    int count_;
    repeating_timer_t timer_{};
    bool running_ = false;
    bool phase_ = false;
};

}  // namespace mc::mcu
