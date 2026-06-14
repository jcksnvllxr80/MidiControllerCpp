#pragma once
//
// McuClock — IClock backed by the RP2350 64-bit microsecond timer.
//
#include "pico/time.h"

#include "mc/ports/IClock.h"

namespace mc::mcu {

class McuClock : public IClock {
public:
    double now() const override { return static_cast<double>(time_us_64()) / 1'000'000.0; }
};

}  // namespace mc::mcu
