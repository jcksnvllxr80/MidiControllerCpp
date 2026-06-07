#include "mc/domain/MidiPedal.h"

#include <stdexcept>

namespace mc {

namespace {
// Mirrors check_value_for_engaged: {name, engaged} collapses to its engaged bool.
Value checkValueForEngaged(const Value& v) {
    if (auto* e = std::get_if<EngagedRef>(&v)) return Value{e->engaged};
    return v;
}
}  // namespace

MidiPedal::MidiPedal(std::string name, int channel, const PedalConfig* config, IMidiOut* out)
    : name_(std::move(name)), channel_(channel), cfg_(config), out_(out) {}

void MidiPedal::emitCC(long cc, long value) {
    out_->send(MidiMessage::controlChange(channel_, static_cast<int>(cc), static_cast<int>(value)));
}

void MidiPedal::emitPC(long program) {
    out_->send(MidiMessage::programChange(channel_, static_cast<int>(program)));
}

long MidiPedal::requireInt(const Value& v, const char* ctx) const {
    if (auto i = asInt(v)) return *i;
    throw std::runtime_error("MidiPedal " + name_ + ": " + ctx + " expected an integer but got '" +
                             toString(v) + "'");
}

// ---- high-level actions -----------------------------------------------------

void MidiPedal::turnOn() {
    if (const Action* a = cfg_->engage()) {
        determineActionMethod(*a, std::monostate{});
        engaged_ = true;
    }
}

void MidiPedal::turnOff() {
    if (const Action* a = cfg_->bypass()) {
        determineActionMethod(*a, std::monostate{});
        engaged_ = false;
    }
}

void MidiPedal::toggle() {
    if (const Action* a = cfg_->toggleBypass()) {
        determineActionMethod(*a, std::monostate{});
        engaged_ = !engaged_;
    }
}

void MidiPedal::setPreset(const Value& preset) {
    const Action* a = cfg_->setPreset();
    if (!a) return;  // no 'Set Preset' configured
    // Python: a missing preset comes in as '' and is a no-op.
    if (isNone(preset)) return;
    if (auto s = asString(preset); s && s->empty()) return;
    determineActionMethod(*a, preset);
    preset_ = preset;
}

void MidiPedal::setTempo(const Value& tempo) {
    const Action* a = cfg_->setTempo();
    if (!a) return;
    if (isNone(tempo)) return;
    if (auto s = asString(tempo); s && s->empty()) return;
    determineActionMethod(*a, tempo);
}

void MidiPedal::setSetting(const std::string& setting) {
    // Python: midi_command_dict.get(setting) — a top-level group name, executed
    // with the setting name itself as the value. Rarely used (no song uses it).
    if (const Action* a = cfg_->group(setting)) {
        determineActionMethod(*a, Value{setting});
    }
}

void MidiPedal::setParams(const Params& params) {
    static const char* kTypes[] = {"Knobs/Switches", "Parameters"};
    for (const auto& [param, value] : params) {
        for (const char* t : kTypes) {
            if (cfg_->paramGroup(t)) {
                if (setParam(param, value, t)) break;  // stop at the first group that handles it
            }
        }
    }
}

bool MidiPedal::setParam(const std::string& param, const Value& value, const std::string& group) {
    const Action* grp = cfg_->paramGroup(group);
    const Action* info = grp ? grp->child(param) : nullptr;
    if (!info) return false;
    return determineParameterMethod(*info, value);
}

// ---- dispatch ---------------------------------------------------------------

void MidiPedal::determineActionMethod(const Action& a, Value value) {
    if (isNone(value) && a.value) value = Value{*a.value};

    if (a.hasTruthyCc()) {  // 'cc' present and non-zero (cc:0 is falsy in Python)
        value = a.func.apply(value);
        emitCC(*a.cc, requireInt(value, "cc value"));
    } else if (a.programChange) {
        value = a.programChange->transform.apply(value);
        emitPC(requireInt(value, "program change value"));
    } else if (a.controlChange) {
        // The computed value becomes the CC *number*; the data byte defaults to 127.
        value = a.controlChange->transform.apply(value);
        emitCC(requireInt(value, "control change cc#"), 127);
    } else if (!a.multi.empty()) {
        handleMulti(a, value);
    }
}

void MidiPedal::handleMulti(const Action& a, Value val) {
    // `val` is threaded (and mutated) across the sub-actions, exactly like the
    // Python loop. For the real Set Preset configs the 'bank' sub-action has
    // cc:0 (falsy), so it is silently skipped and only the 'preset' program
    // change (x % 128) is sent — see docs/midi-protocol.md.
    for (const std::string& name : a.multi) {
        const Action* todo = a.child(name);
        if (!todo) continue;
        if (todo->hasTruthyCc()) {
            val = todo->func.apply(val);
            emitCC(*todo->cc, requireInt(val, "multi cc value"));
        } else if (todo->programChange) {
            val = todo->programChange->transform.apply(val);
            emitPC(requireInt(val, "multi program change value"));
        }
        // else: neither truthy cc nor program change -> skipped (the cc:0 'bank' case)
    }
}

bool MidiPedal::determineParameterMethod(const Action& a, Value value) {
    if (isNone(value) && a.value) value = Value{*a.value};

    if (!a.hasTruthyCc()) return false;  // params only fire on a non-zero cc

    value = a.func.apply(value);
    value = checkValueForEngaged(value);
    if (auto iv = convertToInt(a, value)) {
        emitCC(*a.cc, *iv);
        return true;
    }
    return false;
}

std::optional<long> MidiPedal::convertToInt(const Action& a, const Value& v) const {
    // Python: dict_val = change_dict.get(v, change_dict.get('dict', {}).get(v, None))
    std::optional<long> dictVal;
    if (auto s = asString(v)) {
        if (*s == "on") dictVal = a.on;
        else if (*s == "off") dictVal = a.off;
        else if (*s == "press") dictVal = a.press;
        else if (*s == "release") dictVal = a.release;
        else if (*s == "value") dictVal = a.value;
        else dictVal = a.dictGet(*s);
    }
    if (dictVal) return dictVal;  // int(dict_val)

    // elif change_dict.get('value') == v
    if (a.value && isInt(v) && *a.value == std::get<long>(v)) return std::get<long>(v);

    // elif isinstance(v, bool)
    if (isBool(v)) return std::get<bool>(v) ? a.on : a.off;  // may be nullopt -> None

    // else: range check
    if (isInt(v) && a.min && a.max) {
        long val = std::get<long>(v);
        if (*a.min <= val && val <= *a.max) return val;
        return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace mc
