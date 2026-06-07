#pragma once
//
// ControllerState — the persisted device state from midi_controller.yaml:
// which set/song/part is loaded, tempo, mode, knob colour/brightness, button
// lock, the MIDI channel->pedal table, and the footswitch action map.
//
#include <optional>
#include <string>
#include <vector>

#include "mc/domain/Value.h"

namespace mc {

// One footswitch's actions (button_setup entry).
struct ButtonConfig {
    int button = 0;
    std::string function;       // short-press action
    std::string longPressFunc;  // long-press action ("" if none)
    std::string partnerFunc;    // partner/double-press action ("" if none)
};

// One MIDI channel binding (midi.channels entry that isn't null).
struct ChannelConfig {
    int channel = 0;       // 1..16
    std::string name;      // pedal name
    bool state = false;    // initial engaged state
    Value preset;          // preset number (long) or name (string)
};

struct ControllerState {
    std::string mode = "standard";  // "standard" | "favorite"
    std::string knobColor = "Off";
    int knobBrightness = 0;
    bool buttonsLocked = false;
    int apiPort = 8090;
    int tempo = 0;

    std::string currentSet;
    std::string currentSong;
    std::string currentPart;

    std::vector<ChannelConfig> channels;  // non-null channels, in ascending channel order
    std::vector<ButtonConfig> buttons;    // in ascending button order

    const ChannelConfig* channelForPedal(const std::string& pedalName) const {
        for (const auto& c : channels)
            if (c.name == pedalName) return &c;
        return nullptr;
    }
    const ButtonConfig* button(int n) const {
        for (const auto& b : buttons)
            if (b.button == n) return &b;
        return nullptr;
    }
};

}  // namespace mc
