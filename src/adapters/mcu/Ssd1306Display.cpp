#include "mc/adapters/mcu/Ssd1306Display.h"

#include <vector>

#include "hardware/gpio.h"

#include "mc/adapters/mcu/Ssd1306Font.h"

namespace mc::mcu {

namespace {
constexpr int kCharW = 6;       // 5px glyph + 1px gap
constexpr int kXStart = 2;      // DISPLAY_X_START in OledDisplay
}  // namespace

Ssd1306Display::Ssd1306Display(i2c_inst_t* i2c, uint8_t addr, unsigned sda, unsigned scl)
    : i2c_(i2c), addr_(addr), sda_(sda), scl_(scl) {}

void Ssd1306Display::command(uint8_t c) {
    uint8_t buf[2] = {0x00, c};  // 0x00 = command stream
    i2c_write_blocking(i2c_, addr_, buf, 2, false);
}

void Ssd1306Display::begin() {
    i2c_init(i2c_, 400'000);
    gpio_set_function(sda_, GPIO_FUNC_I2C);
    gpio_set_function(scl_, GPIO_FUNC_I2C);
    gpio_pull_up(sda_);
    gpio_pull_up(scl_);

    static const uint8_t kInit[] = {
        0xAE,                    // display off
        0xD5, 0x80,              // clock div
        0xA8, 0x3F,              // multiplex = 63 (64 rows)
        0xD3, 0x00,              // display offset
        0x40,                    // start line 0
        0x8D, 0x14,              // charge pump on
        0x20, 0x00,              // memory mode = horizontal
        0xA1,                    // segment remap
        0xC8,                    // COM scan dec
        0xDA, 0x12,              // COM pins
        0x81, 0xCF,              // contrast
        0xD9, 0xF1,              // pre-charge
        0xDB, 0x40,              // VCOM detect
        0xA4,                    // resume from RAM
        0xA6,                    // normal (not inverted)
        0xAF,                    // display on
    };
    for (uint8_t c : kInit) command(c);
    clear();
}

void Ssd1306Display::flush() {
    command(0x21);  // column address
    command(0x00);
    command(kWidth - 1);
    command(0x22);  // page address
    command(0x00);
    command(kPages - 1);

    std::vector<uint8_t> buf;
    buf.reserve(1 + sizeof fb_);
    buf.push_back(0x40);  // data stream
    for (uint8_t b : fb_) buf.push_back(b);
    i2c_write_blocking(i2c_, addr_, buf.data(), buf.size(), false);
}

void Ssd1306Display::drawChar(int page, int x, char c) {
    const uint8_t* g = glyph5x7(c);
    for (int col = 0; col < 5; ++col) {
        int px = x + col;
        if (px < 0 || px >= kWidth) continue;
        fb_[page * kWidth + px] = g[col];
    }
}

void Ssd1306Display::drawLine(int page, const std::string& text) {
    if (page < 0 || page >= kPages) return;
    int x = kXStart;
    for (char c : text) {
        if (x + 5 > kWidth) break;  // truncate overflow
        drawChar(page, x, c);
        x += kCharW;
    }
}

void Ssd1306Display::setMessage(const std::string& msg) {
    for (auto& b : fb_) b = 0;
    // Split on " - " into rows, like OledDisplay.draw_left_justified.
    int page = 0;
    size_t start = 0;
    const std::string sep = " - ";
    while (page < kPages) {
        size_t hit = msg.find(sep, start);
        std::string seg = (hit == std::string::npos) ? msg.substr(start) : msg.substr(start, hit - start);
        drawLine(page, seg);
        ++page;
        if (hit == std::string::npos) break;
        start = hit + sep.size();
    }
    flush();
}

void Ssd1306Display::clear() {
    for (auto& b : fb_) b = 0;
    flush();
}

}  // namespace mc::mcu
