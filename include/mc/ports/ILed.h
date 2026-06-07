#pragma once
//
// ILed — the rotary knob's RGB LED. Ports RgbKnob's colour/brightness surface
// (named colours: Off/Blue/Green/Cyan/Red/Magenta/Yellow/White). PWM duty-cycle
// math lives in the adapter.
//
#include <string>

namespace mc {

class ILed {
public:
    virtual ~ILed() = default;
    virtual void setColor(const std::string& color) = 0;
    virtual void setBrightness(int brightness) = 0;  // 0..100
};

}  // namespace mc
