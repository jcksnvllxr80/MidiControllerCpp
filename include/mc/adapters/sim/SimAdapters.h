#pragma once
//
// Sim/desktop adapters — concrete ports for running the core without hardware.
// MIDI/tempo/LED/display just log; the clock is std::chrono; input is scripted;
// config is the filesystem. Swap these for adapters/mcu in Phase 3 and the core
// is unchanged.
//
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "mc/ports/IClock.h"
#include "mc/ports/IConfigTransport.h"
#include "mc/ports/IDisplay.h"
#include "mc/ports/IInput.h"
#include "mc/ports/ILed.h"
#include "mc/ports/IMidiOut.h"
#include "mc/ports/ITempoOut.h"

namespace mc::sim {

class LoggingMidiOut : public IMidiOut {
public:
    explicit LoggingMidiOut(std::ostream& os = std::cout) : os_(os) {}
    void send(const MidiMessage& m) override { os_ << "  [midi]  " << m.toString() << "\n"; }

private:
    std::ostream& os_;
};

class LoggingTempoOut : public ITempoOut {
public:
    explicit LoggingTempoOut(std::ostream& os = std::cout) : os_(os) {}
    void setBpm(double bpm) override {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.1f", bpm);
        os_ << "  [tempo] set " << buf << " BPM\n";
    }
    void tap() override { os_ << "  [tempo] tap\n"; }

private:
    std::ostream& os_;
};

class ConsoleDisplay : public IDisplay {
public:
    explicit ConsoleDisplay(std::ostream& os = std::cout) : os_(os) {}
    void setMessage(const std::string& msg) override {
        last_ = msg;
        os_ << "  [oled]  " << msg << "\n";
    }
    void clear() override { os_ << "  [oled]  (cleared)\n"; }
    const std::string& last() const { return last_; }

private:
    std::ostream& os_;
    std::string last_;
};

class LoggingLed : public ILed {
public:
    explicit LoggingLed(std::ostream& os = std::cout) : os_(os) {}
    void setColor(const std::string& color) override { os_ << "  [led]   color " << color << "\n"; }
    void setBrightness(int b) override { os_ << "  [led]   brightness " << b << "\n"; }

private:
    std::ostream& os_;
};

class ChronoClock : public IClock {
public:
    double now() const override {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
};

// A scripted input source: enqueue events, the loop drains them then ends.
class ScriptedInput : public IInput {
public:
    ScriptedInput& footswitchShort(int button) { return push({InputEvent::Type::FootswitchShort, button, 0}); }
    ScriptedInput& footswitchLong(int button) { return push({InputEvent::Type::FootswitchLong, button, 0}); }
    ScriptedInput& encoderCW() { return push({InputEvent::Type::EncoderCW, 0, 0}); }
    ScriptedInput& encoderCCW() { return push({InputEvent::Type::EncoderCCW, 0, 0}); }
    ScriptedInput& rotaryPress(double seconds) { return push({InputEvent::Type::RotaryPress, 0, seconds}); }
    ScriptedInput& quit() { return push({InputEvent::Type::Quit, 0, 0}); }

    bool poll(InputEvent& out) override {
        if (idx_ >= queue_.size()) return false;
        out = queue_[idx_++];
        return true;
    }

private:
    ScriptedInput& push(const InputEvent& e) {
        queue_.push_back(e);
        return *this;
    }
    std::vector<InputEvent> queue_;
    size_t idx_ = 0;
};

class NullConfigTransport : public IConfigTransport {
public:
    void begin() override {}
    void poll() override {}
};

}  // namespace mc::sim
