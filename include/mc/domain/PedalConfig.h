#pragma once
//
// PedalConfig — a parsed pedal command map (one TimeLine.yaml / BigSky.yaml ...
// file). The original Python kept these as raw nested dicts and read fields by
// name at runtime; we keep the same recursive shape but typed, so MidiPedal can
// dispatch exactly as EffectLoops.MidiPedal did.
//
// An Action mirrors one YAML mapping node. It may carry a MIDI action directly
// (cc / program change / control change / multi) AND/OR contain named child
// Actions (e.g. "Set Preset" has children "bank"/"preset"; "Knobs/Switches" has
// a child per knob). The group nodes (Engage, Bypass, Set Preset, Knobs/Switches
// ...) are just the root Action's children.
//
// The config layer (config/ConfigLoader) builds these from JSON; this header is
// deliberately free of any JSON dependency so the domain stays portable.
//
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mc/domain/Transform.h"

namespace mc {

// A 'program change' / 'control change' sub-mapping. For MIDI only `transform`
// matters; min/max/options are retained for the menu/UI layer.
struct SubAction {
    Transform transform;             // from a nested 'func'
    std::optional<long> min, max;
    std::vector<std::string> options;
};

struct Action {
    std::optional<long> cc, pc, value, on, off, press, release, min, max;
    Transform func;                                       // this node's 'func'
    std::vector<std::pair<std::string, long>> dict;       // ordered 'dict' name->int
    std::optional<SubAction> programChange;               // 'program change'
    std::optional<SubAction> controlChange;               // 'control change'
    std::vector<std::string> multi;                       // ordered child names from {1:.., 2:..}
    std::vector<std::pair<std::string, Action>> children; // ordered named sub-actions

    const Action* child(const std::string& name) const {
        for (const auto& kv : children)
            if (kv.first == name) return &kv.second;
        return nullptr;
    }
    std::optional<long> dictGet(const std::string& key) const {
        for (const auto& kv : dict)
            if (kv.first == key) return kv.second;
        return std::nullopt;
    }
    // Mirrors the Python truthiness `if action_dict.get('cc'):` — 0 is falsy.
    bool hasTruthyCc() const { return cc.has_value() && *cc != 0; }
};

class PedalConfig {
public:
    Action root;  // root.children = the top-level groups

    const Action* group(const std::string& name) const { return root.child(name); }
    const Action* engage() const { return group("Engage"); }
    const Action* bypass() const { return group("Bypass"); }
    const Action* toggleBypass() const { return group("Toggle Bypass"); }
    const Action* setPreset() const { return group("Set Preset"); }
    const Action* setTempo() const { return group("Set Tempo"); }
    // "Knobs/Switches" or "Parameters"
    const Action* paramGroup(const std::string& which) const { return group(which); }
};

}  // namespace mc
