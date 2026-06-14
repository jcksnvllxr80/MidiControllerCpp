#pragma once
//
// Ssd1306Display — IDisplay for a 128x64 SSD1306 over I2C. Ports OledDisplay's
// behaviour: split the message on " - " and draw each segment left-justified on
// its own text row (mirrors draw_left_justified). Font rows are 8px tall.
//
#include <cstdint>
#include <string>

#include "hardware/i2c.h"

#include "mc/ports/IDisplay.h"

namespace mc::mcu {

class Ssd1306Display : public IDisplay {
public:
    Ssd1306Display(i2c_inst_t* i2c, uint8_t addr, unsigned sda, unsigned scl);
    void begin();  // init I2C + panel
    void setMessage(const std::string& msg) override;
    void clear() override;

private:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;
    static constexpr int kPages = kHeight / 8;

    void command(uint8_t c);
    void flush();
    void drawChar(int page, int x, char c);
    void drawLine(int page, const std::string& text);

    i2c_inst_t* i2c_;
    uint8_t addr_;
    unsigned sda_, scl_;
    uint8_t fb_[kWidth * kPages] = {0};
};

}  // namespace mc::mcu
