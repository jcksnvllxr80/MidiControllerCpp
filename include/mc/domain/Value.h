#pragma once
//
// Value — the dynamically-typed value that flows through the MIDI dispatch
// pipeline. The original Python (EffectLoops.py) leaned on duck typing: a
// pedal "value" could be an int, a string (a dict key or preset name), a bool,
// "None", or a small dict like {"name": ..., "engaged": ...}. We model exactly
// those shapes so the dispatch logic ports 1:1.
//
#include <optional>
#include <string>
#include <variant>

namespace mc {

// A param value expressed in a song as {name, engaged} (e.g. SuperSwitcher
// loops). Mirrors Python's check_value_for_engaged: only `engaged` affects the
// MIDI sent; `name` is purely descriptive.
struct EngagedRef {
    std::string name;
    bool engaged = false;
    bool operator==(const EngagedRef& o) const { return name == o.name && engaged == o.engaged; }
    bool operator!=(const EngagedRef& o) const { return !(*this == o); }
};

// std::monostate == Python's None.
using Value = std::variant<std::monostate, bool, long, std::string, EngagedRef>;

inline bool isNone(const Value& v) { return std::holds_alternative<std::monostate>(v); }
inline bool isBool(const Value& v) { return std::holds_alternative<bool>(v); }
inline bool isInt(const Value& v) { return std::holds_alternative<long>(v); }
inline bool isString(const Value& v) { return std::holds_alternative<std::string>(v); }
inline bool isEngagedRef(const Value& v) { return std::holds_alternative<EngagedRef>(v); }

inline std::optional<long> asInt(const Value& v) {
    if (auto* p = std::get_if<long>(&v)) return *p;
    return std::nullopt;
}
inline std::optional<std::string> asString(const Value& v) {
    if (auto* p = std::get_if<std::string>(&v)) return *p;
    return std::nullopt;
}

// Human-readable form for logs and display, roughly str(v) in Python.
std::string toString(const Value& v);

}  // namespace mc
