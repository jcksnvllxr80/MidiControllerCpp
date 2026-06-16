#pragma once
//
// Ssd1306Display — IDisplay for a 128x64 SSD1306 over 4-wire SPI (the bus the
// original PCB uses: the Pi drove this panel over SPI, not I2C). Ports
// OledDisplay's behaviour: split the message on " - " and draw each segment
// left-justified on its own 8px text row (mirrors draw_left_justified).
//
#include <cstdint>
#include <string>

#include "hardware/spi.h"

#include "mc/ports/IDisplay.h"

namespace mc::mcu {

class Ssd1306Display : public IDisplay {
public:
    Ssd1306Display(spi_inst_t* spi, unsigned sclk, unsigned mosi, unsigned cs, unsigned dc, unsigned rst);
    void begin();  // init SPI + panel (reset pulse, init sequence)
    void setMessage(const std::string& msg) override;
    void clear() override;

private:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;
    static constexpr int kPages = kHeight / 8;

    void command(uint8_t c);
    void writeData(const uint8_t* data, size_t len);
    void flush();
    void drawChar(int page, int x, char c);
    void drawLine(int page, const std::string& text);

    spi_inst_t* spi_;
    unsigned sclk_, mosi_, cs_, dc_, rst_;
    uint8_t fb_[kWidth * kPages] = {0};
};

}  // namespace mc::mcu
