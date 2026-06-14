#include "mc/adapters/mcu/McuLed.h"

#include "hardware/gpio.h"
#include "hardware/pwm.h"

namespace mc::mcu {

namespace {
// Maps a colour name to on/off per channel (RgbKnob.set_color).
void rgbFor(const std::string& c, bool& r, bool& g, bool& b) {
    r = g = b = false;
    if (c == "Blue") b = true;
    else if (c == "Green") g = true;
    else if (c == "Cyan") { g = b = true; }
    else if (c == "Red") r = true;
    else if (c == "Magenta") { r = b = true; }
    else if (c == "Yellow") { r = g = true; }
    else if (c == "White") { r = g = b = true; }
    // "Off" (or unknown) -> all false
}
void initChannel(unsigned pin) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    unsigned slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, 100);  // duty in whole percent
    pwm_set_enabled(slice, true);
}
}  // namespace

McuLed::McuLed(unsigned rPin, unsigned gPin, unsigned bPin, bool commonAnode)
    : rPin_(rPin), gPin_(gPin), bPin_(bPin), anode_(commonAnode) {}

void McuLed::begin() {
    initChannel(rPin_);
    initChannel(gPin_);
    initChannel(bPin_);
    apply();
}

void McuLed::setColor(const std::string& color) {
    color_ = color;
    apply();
}

void McuLed::setBrightness(int brightness) {
    brightness_ = brightness < 0 ? 0 : (brightness > 100 ? 100 : brightness);
    apply();
}

void McuLed::apply() {
    bool r, g, b;
    rgbFor(color_, r, g, b);
    auto level = [&](bool on) {
        int v = on ? brightness_ : 0;
        return static_cast<uint16_t>(anode_ ? (100 - v) : v);  // common-anode inverts duty
    };
    pwm_set_gpio_level(rPin_, level(r));
    pwm_set_gpio_level(gPin_, level(g));
    pwm_set_gpio_level(bPin_, level(b));
}

}  // namespace mc::mcu
