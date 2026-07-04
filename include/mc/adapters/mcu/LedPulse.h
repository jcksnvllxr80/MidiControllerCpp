#pragma once
//
// LedPulse — momentary flash of the Pico 2 W's onboard LED to signal activity
// (a handled input or command). The onboard LED is NOT a GPIO on the W boards:
// it hangs off the CYW43 wireless chip and is driven via cyw43_arch_gpio_put,
// so it must only be touched from the main core after cyw43_arch_init (done by
// WifiManager::begin). The pulse is non-blocking — trigger() lights it and
// stamps an off-time; update() (called every main-loop pass) clears it once the
// window elapses. No sleep, no second core, no timer IRQ.
//
#include <cstdint>

#include "pico/cyw43_arch.h"
#include "pico/time.h"

namespace mc::mcu {

class LedPulse {
public:
    explicit LedPulse(uint32_t pulseMs = 50) : pulseMs_(pulseMs) {}

    void trigger() {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        on_ = true;
        offAtMs_ = to_ms_since_boot(get_absolute_time()) + pulseMs_;
    }

    void update() {
        if (on_ && to_ms_since_boot(get_absolute_time()) >= offAtMs_) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            on_ = false;
        }
    }

private:
    uint32_t pulseMs_;
    uint32_t offAtMs_ = 0;
    bool on_ = false;
};

}  // namespace mc::mcu
