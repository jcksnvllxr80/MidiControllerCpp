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

#include "mc/adapters/mcu/SpleenFont.h"
#include "mc/ports/IDisplay.h"

namespace mc::mcu {

class Ssd1306Display : public IDisplay {
public:
    Ssd1306Display(spi_inst_t* spi, unsigned sclk, unsigned mosi, unsigned cs, unsigned dc, unsigned rst);
    void begin();  // init SPI + panel (reset pulse, init sequence)
    void setMessage(const std::string& msg) override;
    void clear() override;

    // Drive the title marquee. Call every main-loop pass with the current time;
    // when the title is wider than the screen it scrolls (hold, scroll, hold,
    // reset). A no-op when the title already fits.
    void tick(double nowSec);

private:
    static constexpr int kWidth = 128;
    static constexpr int kHeight = 64;
    static constexpr int kPages = kHeight / 8;

    // Crisp native bitmap fonts (Spleen). Distinct sizes give the hierarchy:
    // title (12x24) > part (8x16) > BPM (6x12). No scaling artefacts.
    static const BitmapFont& titleFont() { return kSpleen12x24; }
    static constexpr int kTitleY = 0;
    static constexpr int kDividerY = 26;
    static constexpr int kBodyY0 = 30;
    static constexpr int kMaxBody = 3;
    static constexpr int kMargin = 2;

    void command(uint8_t c);
    void writeData(const uint8_t* data, size_t len);
    void flush();
    void setPixel(int x, int y);
    void drawGlyph(int x, int y, const BitmapFont& f, char c);
    static int textWidth(const std::string& text, const BitmapFont& f);
    void drawText(int x, int y, const std::string& text, const BitmapFont& f);
    void drawCentered(int y, const std::string& text, const BitmapFont& f);
    static const BitmapFont& bodyFont(const std::string& line);  // BPM -> smaller
    void render(int titleOffset);  // rebuild the framebuffer + flush

    spi_inst_t* spi_;
    unsigned sclk_, mosi_, cs_, dc_, rst_;
    uint8_t fb_[kWidth * kPages] = {0};

    // Parsed message + marquee state.
    std::string title_;
    std::string body_[kMaxBody];
    int bodyCount_ = 0;
    int titlePxWidth_ = 0;
    int scrollOffset_ = -1;     // last rendered title offset (-1 = force render)
    double phaseStart_ = -1.0;  // start of current marquee cycle (-1 = uninit)
};

}  // namespace mc::mcu
