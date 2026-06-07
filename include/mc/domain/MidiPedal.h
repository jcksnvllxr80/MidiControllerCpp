#pragma once
//
// MidiPedal — one MIDI-controlled pedal. Ports EffectLoops.MidiPedal: it turns
// high-level actions (engage, bypass, set preset, set tempo, tweak a param) into
// exact MIDI bytes, emitted through IMidiOut. The dispatch (determine_action_method
// / determine_parameter_method / convert_to_int / handle_multi) is reproduced
// faithfully, including a couple of load-bearing Python quirks documented inline.
//
// Unlike the Python ctor (which emitted MIDI for the pedal's default state as a
// side effect of construction), this ctor is side-effect free: the Application
// drives all emissions via loadPart. See docs/domain.md.
//
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mc/domain/PedalConfig.h"
#include "mc/domain/Value.h"
#include "mc/ports/IMidiOut.h"

namespace mc {

class MidiPedal {
public:
    using Params = std::vector<std::pair<std::string, Value>>;

    MidiPedal(std::string name, int channel, const PedalConfig* config, IMidiOut* out);

    const std::string& name() const { return name_; }
    int channel() const { return channel_; }
    bool isEngaged() const { return engaged_; }
    const Value& preset() const { return preset_; }

    void turnOn();
    void turnOff();
    void toggle();
    void setPreset(const Value& preset);
    void setTempo(const Value& tempo);
    void setParams(const Params& params);
    void setSetting(const std::string& setting);

private:
    // dispatch (mirrors the Python methods of the same intent)
    void determineActionMethod(const Action& a, Value value);
    bool determineParameterMethod(const Action& a, Value value);
    bool setParam(const std::string& param, const Value& value, const std::string& group);
    std::optional<long> convertToInt(const Action& a, const Value& v) const;
    void handleMulti(const Action& a, Value val);

    void emitCC(long cc, long value);
    void emitPC(long program);
    long requireInt(const Value& v, const char* ctx) const;

    std::string name_;
    int channel_;
    const PedalConfig* cfg_;
    IMidiOut* out_;
    bool engaged_ = false;
    Value preset_;
};

}  // namespace mc
