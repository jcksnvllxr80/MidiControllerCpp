#pragma once
// Recording/null port adapters for end-to-end assertions.
#include <string>
#include <vector>

#include "mc/ports/IDisplay.h"
#include "mc/ports/ILed.h"
#include "mc/ports/ITempoOut.h"

namespace mc::test {

class RecordingDisplay : public IDisplay {
public:
    void setMessage(const std::string& msg) override { messages.push_back(msg); }
    void clear() override { cleared++; }
    const std::string& last() const { return messages.back(); }
    std::vector<std::string> messages;
    int cleared = 0;
};

class RecordingTempoOut : public ITempoOut {
public:
    void setBpm(double bpm) override { bpms.push_back(bpm); }
    void tap() override { taps++; }
    std::vector<double> bpms;
    int taps = 0;
};

class NullLed : public ILed {
public:
    void setColor(const std::string&) override {}
    void setBrightness(int) override {}
};

}  // namespace mc::test
