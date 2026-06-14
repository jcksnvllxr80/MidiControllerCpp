#pragma once
//
// McuLed — ILed for the knob's RGB LED via 3 PWM channels. Colour names match
// RgbKnob (Off/Blue/Green/Cyan/Red/Magenta/Yellow/White); brightness is 0..100.
//
#include <string>

#include "mc/ports/ILed.h"

namespace mc::mcu {

class McuLed : public ILed {
public:
    McuLed(unsigned rPin, unsigned gPin, unsigned bPin, bool commonAnode = false);
    void begin();  // call once before use
    void setColor(const std::string& color) override;
    void setBrightness(int brightness) override;

private:
    void apply();
    unsigned rPin_, gPin_, bPin_;
    bool anode_;
    int brightness_ = 100;
    std::string color_ = "Off";
};

}  // namespace mc::mcu
