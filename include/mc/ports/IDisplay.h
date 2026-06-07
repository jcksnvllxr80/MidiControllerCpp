#pragma once
//
// IDisplay — the OLED. Ports OledDisplay's surface down to what the brain
// actually uses: show a message, clear the screen. Word-wrap / fonts / SSD1306
// SPI live in the adapter.
//
#include <string>

namespace mc {

class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual void setMessage(const std::string& msg) = 0;
    virtual void clear() = 0;
};

}  // namespace mc
