#include "mc/adapters/mcu/Ssd1306Display.h"

#include "hardware/gpio.h"
#include "pico/stdlib.h"  // sleep_ms

#include "mc/adapters/mcu/Log.h"
#include "mc/adapters/mcu/Ssd1306Font.h"

namespace mc::mcu {

namespace {
constexpr int kGlyphW = 5;   // glyph pixel columns
constexpr int kGlyphH = 8;   // glyph pixel rows (row 7 holds descenders: g/y/q/p)
constexpr int kAdvance = 6;  // glyph + 1px gap, at native size
}  // namespace

Ssd1306Display::Ssd1306Display(spi_inst_t* spi, unsigned sclk, unsigned mosi, unsigned cs, unsigned dc,
                               unsigned rst)
    : spi_(spi), sclk_(sclk), mosi_(mosi), cs_(cs), dc_(dc), rst_(rst) {}

void Ssd1306Display::command(uint8_t c) {
    gpio_put(dc_, 0);  // DC low = command
    gpio_put(cs_, 0);
    spi_write_blocking(spi_, &c, 1);
    gpio_put(cs_, 1);
}

void Ssd1306Display::writeData(const uint8_t* data, size_t len) {
    gpio_put(dc_, 1);  // DC high = display data
    gpio_put(cs_, 0);
    spi_write_blocking(spi_, data, len);
    gpio_put(cs_, 1);
}

void Ssd1306Display::begin() {
    spi_init(spi_, 10'000'000);  // 10 MHz
    gpio_set_function(sclk_, GPIO_FUNC_SPI);
    gpio_set_function(mosi_, GPIO_FUNC_SPI);
    for (unsigned pin : {cs_, dc_, rst_}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    gpio_put(cs_, 1);

    // Reset pulse.
    gpio_put(rst_, 1);
    sleep_ms(1);
    gpio_put(rst_, 0);
    sleep_ms(10);
    gpio_put(rst_, 1);
    sleep_ms(10);

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
    LOG_I("oled", "SSD1306 128x64 ready (SPI 10MHz)");
}

void Ssd1306Display::flush() {
    command(0x21);  // column address
    command(0x00);
    command(kWidth - 1);
    command(0x22);  // page address
    command(0x00);
    command(kPages - 1);
    writeData(fb_, sizeof fb_);
}

void Ssd1306Display::setPixel(int x, int y) {
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) return;
    fb_[(y >> 3) * kWidth + x] |= static_cast<uint8_t>(1u << (y & 7));
}

// Draw one bitmap glyph at top-left (x,y). Row-major, MSB = leftmost pixel.
void Ssd1306Display::drawGlyph(int x, int y, const BitmapFont& f, char c) {
    const uint8_t* g = f.glyph(c);
    for (int row = 0; row < f.h; ++row) {
        const uint8_t* r = g + row * f.bytesPerRow;
        for (int col = 0; col < f.w; ++col) {
            if ((r[col >> 3] >> (7 - (col & 7))) & 1u) setPixel(x + col, y + row);
        }
    }
}

int Ssd1306Display::textWidth(const std::string& text, const BitmapFont& f) {
    return static_cast<int>(text.size()) * f.w;  // monospace
}

// Draw `text` with its left edge at x (may be negative / run off-screen — pixels
// outside the panel are clipped by setPixel). Used by the title marquee.
void Ssd1306Display::drawText(int x, int y, const std::string& text, const BitmapFont& f) {
    for (char c : text) {
        drawGlyph(x, y, f, c);
        x += f.w;
    }
}

// Draw `text` horizontally centered on row `y`; auto-truncates if it overflows.
void Ssd1306Display::drawCentered(int y, const std::string& text, const BitmapFont& f) {
    int w = textWidth(text, f);
    int x = (w >= kWidth) ? 0 : (kWidth - w) / 2;
    for (char c : text) {
        if (x + f.w > kWidth) break;  // clip overflow
        drawGlyph(x, y, f, c);
        x += f.w;
    }
}

// Body lines: the "<n>BPM" field is the smaller font, everything else (the part
// name, or a menu item) gets the larger one.
const BitmapFont& Ssd1306Display::bodyFont(const std::string& line) {
    bool isBpm = line.size() >= 3 && line.compare(line.size() - 3, 3, "BPM") == 0;
    return isBpm ? kSpleen6x12 : kSpleen8x16;
}

void Ssd1306Display::render(int titleOffset) {
    for (auto& b : fb_) b = 0;

    // Title: centered when it fits, else left-aligned and scrolled by the marquee.
    if (titlePxWidth_ <= kWidth - 2 * kMargin)
        drawCentered(kTitleY, title_, titleFont());
    else
        drawText(kMargin - titleOffset, kTitleY, title_, titleFont());

    // Divider rule, inset from the edges.
    for (int x = 6; x < kWidth - 6; ++x) setPixel(x, kDividerY);

    // Body lines: each sized by its kind (part > BPM), centered, stacked below
    // the rule with a running cursor so mixed sizes don't overlap.
    int y = kBodyY0;
    for (int i = 0; i < bodyCount_; ++i) {
        const BitmapFont& bf = bodyFont(body_[i]);
        drawCentered(y, body_[i], bf);
        y += bf.h + 2;  // line height + 2px gap
    }

    flush();
}

void Ssd1306Display::setMessage(const std::string& msg) {
    // Only redraw when the content actually changes. A repeated identical message
    // (e.g. an encoder turn rejected at a list boundary, or a re-emit of the same
    // screen) must NOT trigger a full framebuffer rebuild + blocking SPI flush —
    // that blocking work is what was stranding the encoder IRQ debounce state.
    if (msg == lastMsg_) return;
    lastMsg_ = msg;
    LOG_I("oled", "msg: %s", msg.c_str());
    // Split on " - " into segments. The title is everything up to the "<n>BPM"
    // field (so a song name that itself contains " - <sub-desc>", e.g.
    // "Victory - Rhythm", stays whole on the title line); BPM and the part name
    // become the body. Messages without a BPM field (menus) fall back to
    // segment[0] = title, the rest = body.
    const std::string sep = " - ";
    std::string segs[8];
    int n = 0;
    size_t start = 0;
    while (n < 8) {
        size_t hit = msg.find(sep, start);
        segs[n++] = (hit == std::string::npos) ? msg.substr(start) : msg.substr(start, hit - start);
        if (hit == std::string::npos) break;
        start = hit + sep.size();
    }

    int titleEnd = 1;  // number of leading segments that form the title
    for (int i = 0; i < n; ++i) {
        const std::string& s = segs[i];
        if (s.size() >= 3 && s.compare(s.size() - 3, 3, "BPM") == 0) {
            titleEnd = i;  // title is everything before the BPM field
            break;
        }
    }
    if (titleEnd < 1) titleEnd = 1;  // always keep at least the first segment

    title_.clear();
    for (int i = 0; i < titleEnd; ++i) {
        if (i) title_ += sep;
        title_ += segs[i];
    }
    bodyCount_ = 0;
    for (int i = titleEnd; i < n && bodyCount_ < kMaxBody; ++i) body_[bodyCount_++] = segs[i];

    titlePxWidth_ = textWidth(title_, titleFont());
    scrollOffset_ = 0;
    phaseStart_ = -1.0;  // tick() re-arms the marquee timing on its next pass
    render(0);
}

void Ssd1306Display::tick(double nowSec) {
    constexpr double kHold = 2.0;        // seconds paused at each end
    constexpr double kPxPerSec = 27.0;   // marquee scroll speed (1.5x of the original 18)
    const int maxOffset = titlePxWidth_ - (kWidth - 2 * kMargin);
    if (maxOffset <= 0) {                // title fits — nothing to scroll
        if (scrollOffset_ != 0) { scrollOffset_ = 0; render(0); }
        return;
    }
    if (phaseStart_ < 0) phaseStart_ = nowSec;
    double t = nowSec - phaseStart_;
    double scrollDur = maxOffset / kPxPerSec;

    int offset;
    if (t < kHold)
        offset = 0;                                    // hold at start
    else if (t < kHold + scrollDur)
        offset = static_cast<int>((t - kHold) * kPxPerSec);  // scrolling
    else if (t < kHold + scrollDur + kHold)
        offset = maxOffset;                            // hold at end
    else {
        phaseStart_ = nowSec;                          // cycle restarts
        offset = 0;
    }
    if (offset < 0) offset = 0;
    if (offset > maxOffset) offset = maxOffset;
    if (offset != scrollOffset_) { scrollOffset_ = offset; render(offset); }
}

void Ssd1306Display::clear() {
    lastMsg_.clear();  // force the next setMessage to redraw, even if identical
    title_.clear();
    bodyCount_ = 0;
    titlePxWidth_ = 0;
    scrollOffset_ = 0;
    for (auto& b : fb_) b = 0;
    flush();
}

}  // namespace mc::mcu
