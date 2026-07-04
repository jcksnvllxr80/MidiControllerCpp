#include "mc/app/Application.h"

#include <algorithm>
#include <exception>

#include "mc/config/ConfigLoader.h"

namespace mc {

namespace {
int indexOf(const std::vector<std::string>& v, const std::string& s, int dflt = 0) {
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] == s) return static_cast<int>(i);
    return dflt;
}
// Idle gap after the last set/song/part change before "save defaults" hits flash.
// Debounced so rapid footswitch part changes during a song don't stall the rig on
// an IRQ-masked flash write mid-performance.
constexpr double kDefaultsPersistDelaySec = 1.5;

// The selectable values for one editable parameter leaf, mirroring the Python
// parse_option_dict display rules (RotaryEncoder.py): a 'dict' shows its keys
// (sorted by MIDI value, ascending), a min/max shows the integer range, on/off
// and press/release show their two labels, and a 'control change' shows its
// options. Returns empty when the action carries no user-choosable value.
std::vector<std::string> paramItems(const Action& a) {
    std::vector<std::string> items;
    if (!a.dict.empty()) {
        auto sorted = a.dict;  // copy: (name, value) pairs in JSON order
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const auto& l, const auto& r) { return l.second < r.second; });
        for (const auto& kv : sorted) items.push_back(kv.first);
    } else if (a.min && a.max) {
        for (long v = *a.min; v <= *a.max; ++v) items.push_back(std::to_string(v));
    } else if (a.on && a.off) {
        items = {"off", "on"};
    } else if (a.press && a.release) {
        items = {"press", "release"};
    } else if (a.controlChange && !a.controlChange->options.empty()) {
        items = a.controlChange->options;
    }
    return items;
}

// Turn a selected menu item back into the Value MidiPedal expects. The dict /
// on-off / press-release / control-change branches are string-keyed (convertToInt
// looks them up by name); only the bare min/max range is a literal integer. This
// branch order matches paramItems exactly, so a numeric-looking dict key is never
// mistaken for a range integer.
Value paramValue(const Action& a, const std::string& item) {
    const bool stringKeyed = !a.dict.empty() || (a.on && a.off) || (a.press && a.release) ||
                             (a.controlChange && !a.controlChange->options.empty());
    if (stringKeyed) return Value{item};
    return Value{static_cast<long>(std::stol(item))};
}
}  // namespace

Application::Application(Ports ports) : p_(ports) {}

void Application::setup() {
    // Boot must never crash-loop on bad data: if the controller config or setlist
    // won't parse, fall back to an empty-but-runnable shell so the editor link can
    // still connect and fix it. (The committed data is good; this guards a corrupt
    // flash overlay or a half-finished write.)
    try {
        state_ = config::loadControllerStateFromString(p_.store->read("midi_controller.json"));
        loadSetlist(state_.currentSet);
    } catch (const std::exception&) {
        setupFailed_ = true;
        setlist_ = Setlist{};
    }

    currentSongIdx_ = std::max(0, setlist_.indexOfSong(state_.currentSong));
    currentPartIdx_ = std::max(0, currentSong().indexOfPart(state_.currentPart));
    displayedSongIdx_ = currentSongIdx_;
    displayedPartIdx_ = currentPartIdx_;

    // Build MidiPedals in channel order. The PedalConfig objects live in
    // pedalConfigs_ (std::map -> stable addresses), referenced by the pedals.
    // A single malformed pedal file degrades to "that pedal missing", not a crash.
    for (const auto& ch : state_.channels) {
        try {
            auto it = pedalConfigs_.emplace(ch.name, config::loadPedalConfigFromString(
                                                         p_.store->read("pedals/" + ch.name + ".json")))
                          .first;
            pedals_.push_back(std::make_unique<MidiPedal>(ch.name, ch.channel, &it->second, p_.midi));
        } catch (const std::exception&) {
            setupFailed_ = true;  // skip this pedal, keep building the rest
        }
    }

    if (p_.led) {
        p_.led->setColor(state_.knobColor);
        p_.led->setBrightness(state_.knobBrightness);
    }
    buildMenu();
    if (setupFailed_ && setlist_.songs.empty())
        show("Config error - use editor to fix");
    else
        setSongInfoMessage();  // standard mode: show current song info, no MIDI at boot
}

void Application::loadSetlist(const std::string& setName) {
    setlist_ = config::loadSetlistFromString(
        p_.store->read("sets/" + setName + ".json"),
        [this](const std::string& songName) { return p_.store->read("songs/" + songName + ".json"); });
}

void Application::run() {
    if (p_.transport) p_.transport->begin();
    InputEvent ev;
    while (!quitRequested_ && p_.input->poll(ev)) {
        if (p_.transport) p_.transport->poll();
        if (!handleEvent(ev)) break;
    }
}

bool Application::handleEvent(const InputEvent& ev) {
    // Flash the activity indicator for any real input/command (knob turn,
    // footswitch, rotary press, editor-driven change) — not idle/Quit polls.
    if (activitySink_ && ev.type != InputEvent::Type::None && ev.type != InputEvent::Type::Quit)
        activitySink_();

    switch (ev.type) {
        case InputEvent::Type::FootswitchShort:
            if (!state_.buttonsLocked) {
                if (const ButtonConfig* b = state_.button(ev.button)) buttonExecutor(b->function);
            }
            break;
        case InputEvent::Type::FootswitchLong:
            if (!state_.buttonsLocked) {
                if (const ButtonConfig* b = state_.button(ev.button))
                    if (!b->longPressFunc.empty()) changeAndSelect(b->longPressFunc);
            }
            break;
        case InputEvent::Type::EncoderCW:
            menu_.changeMenuPos("CW");
            break;
        case InputEvent::Type::EncoderCCW:
            menu_.changeMenuPos("CCW");
            break;
        case InputEvent::Type::RotaryPress:
            menu_.pressFor(ev.holdSeconds);
            break;
        case InputEvent::Type::Quit:
            return false;
        case InputEvent::Type::None:
            break;
    }
    return !quitRequested_;
}

// --- footswitch action maps (ports button_executor / change_and_select) ------

void Application::buttonExecutor(const std::string& fn) {
    if (fn == "Song Dn") prevSong();
    else if (fn == "Song Up") nextSong();
    else if (fn == "Part Dn") prevPart();
    else if (fn == "Part Up") nextPart();
    else if (fn == "Select") selectChoice();
}

void Application::changeAndSelect(const std::string& fn) {
    bool matched = true;
    if (fn == "Select Song Dn") prevSong();
    else if (fn == "Select Song Up") nextSong();
    else if (fn == "Select Part Dn") prevPart();
    else if (fn == "Select Part Up") nextPart();
    else matched = false;
    if (matched) selectChoice();
}

// --- navigation (ports RotaryEncoder) ----------------------------------------

void Application::nextSong() {
    if (displayedSongIdx_ < static_cast<int>(setlist_.songs.size()) - 1) {
        ++displayedSongIdx_;
        displayedPartIdx_ = 0;
        previewMessage();
    }
}
void Application::prevSong() {
    if (displayedSongIdx_ > 0) {
        --displayedSongIdx_;
        displayedPartIdx_ = 0;
        previewMessage();
    }
}
void Application::nextPart() {
    if (displayedPartIdx_ < static_cast<int>(displayedSong().parts.size()) - 1) {
        ++displayedPartIdx_;
        previewMessage();
    }
}
void Application::prevPart() {
    if (displayedPartIdx_ > 0) {
        --displayedPartIdx_;
        previewMessage();
    }
}

void Application::selectChoice() {
    if (currentSongIdx_ != displayedSongIdx_) {
        loadSong();
    } else if (currentPartIdx_ != displayedPartIdx_) {
        currentPartIdx_ = displayedPartIdx_;
        loadPart();
    }
}

void Application::loadSong() {
    currentSongIdx_ = displayedSongIdx_;
    currentPartIdx_ = displayedPartIdx_;
    loadPart();
}

void Application::loadPart() {
    const Part& part = currentPart();
    for (auto& pedal : pedals_) {
        const PedalState* st = part.pedal(pedal->name());
        if (!st) continue;  // pedal not in this part
        if (st->engaged) pedal->turnOn();
        else pedal->turnOff();
        pedal->setPreset(st->preset);  // no-op on empty/None
        if (!st->params.empty()) pedal->setParams(st->params);
        if (auto s = asString(st->settings); s && !s->empty()) pedal->setSetting(*s);
    }
    setSongInfoMessage();

    // Remember this committed selection as the boot default. (currentSet is set by
    // the Sets menu; song/part funnel through here from every commit path.)
    state_.currentSong = currentSong().name;
    state_.currentPart = currentPart().name;
    markDefaultsDirty();
}

// --- "save defaults" persistence (ports midi_controller.yaml rewrite) ---------

void Application::tick() {
    if (defaultsDirty_ && p_.clock && (p_.clock->now() - defaultsDirtyAt_) >= kDefaultsPersistDelaySec)
        persistDefaults();
}

void Application::markDefaultsDirty() {
    if (p_.clock) {
        defaultsDirty_ = true;
        defaultsDirtyAt_ = p_.clock->now();  // flushed by tick() after an idle gap
    } else {
        persistDefaults();  // no clock to debounce against -> write through now
    }
}

void Application::persistDefaults() {
    defaultsDirty_ = false;
    if (!p_.store) return;
    try {
        std::string updated = config::updateControllerDefaults(p_.store->read("midi_controller.json"),
                                                               state_.currentSet, state_.currentSong,
                                                               state_.currentPart);
        p_.store->write("midi_controller.json", updated);
    } catch (const std::exception&) {
        // Best-effort: a failed defaults save must never take down the rig.
    }
}

// --- display messages --------------------------------------------------------

std::string Application::songInfoString(const Song& s, const Part& p) const {
    return s.name + " - " + s.bpm + "BPM - " + p.name;
}
void Application::show(const std::string& msg) {
    lastMessage_ = msg;  // so displayedMessage() (and the editor link) can read it back
    if (p_.display) p_.display->setMessage(msg);
}
void Application::setSongInfoMessage() { show(songInfoString(currentSong(), currentPart())); }
void Application::previewMessage() { show(songInfoString(displayedSong(), displayedPart())); }

// --- menu wiring -------------------------------------------------------------

void Application::buildMenu() {
    menu_.setSink([this](const std::string& m) { show(m); });
    menu_.setRootMessage([this] { return songInfoString(currentSong(), currentPart()); });

    setupMenu_ = menu_.root()->addChild("Setup");
    globalMenu_ = menu_.root()->addChild("Global");
    powerMenu_ = menu_.root()->addChild("Power", [this] {
        powerMenu_->dataItems = {"NO", "YES"};
        powerMenu_->dataPrompt = "Power Off?";
        powerMenu_->dataPosition = 0;
        powerMenu_->dataDict["NO"] = [this] { menu_.changeMenuNodes(); };
        powerMenu_->dataDict["YES"] = [this] { quitRequested_ = true; };
    });
    menu_.setAnchors(setupMenu_, globalMenu_, powerMenu_);

    // ----- Setup: Sets / Songs / Parts -----
    setsNode_ = setupMenu_->addChild(
        "Sets",
        [this] {
            setsNode_->dataItems = p_.store->list("sets");
            setsNode_->dataPrompt = "Sets:";
            setsNode_->dataPosition = indexOf(setsNode_->dataItems, state_.currentSet);
        },
        [this] {
            const std::string setName = setsNode_->dataItems[setsNode_->dataPosition];
            loadSetlist(setName);
            state_.currentSet = setName;
            currentSongIdx_ = currentPartIdx_ = displayedSongIdx_ = displayedPartIdx_ = 0;
            loadPart();
            menu_.changeMenuNodes();
        });

    songsNode_ = setupMenu_->addChild(
        "Songs",
        [this] {
            songsNode_->dataItems.clear();
            for (const auto& s : setlist_.songs) songsNode_->dataItems.push_back(s.name);
            songsNode_->dataPrompt = "Songs:";
            songsNode_->dataPosition = displayedSongIdx_;
        },
        [this] {
            displayedSongIdx_ = songsNode_->dataPosition;
            displayedPartIdx_ = 0;
            loadSong();
            menu_.changeMenuNodes();
        });

    partsNode_ = setupMenu_->addChild(
        "Parts",
        [this] {
            partsNode_->dataItems.clear();
            for (const auto& part : currentSong().parts) partsNode_->dataItems.push_back(part.name);
            partsNode_->dataPrompt = "Parts:";
            partsNode_->dataPosition = currentPartIdx_;
        },
        [this] {
            currentPartIdx_ = displayedPartIdx_ = partsNode_->dataPosition;
            loadPart();
            menu_.changeMenuNodes();
        });

    buildPedalMenu();  // Setup -> Midi Pedals -> pedal -> group -> param

    // ----- Global: Knob Color / Brightness / Button Lock -----
    static const std::vector<std::string> kColors = {"Off", "Blue",    "Green",  "Cyan",
                                                     "Red", "Magenta", "Yellow", "White"};
    colorNode_ = globalMenu_->addChild(
        "Knob Color",
        [this] {
            colorNode_->dataItems = kColors;
            colorNode_->dataPrompt = "Knob Color:";
            colorNode_->dataPosition = indexOf(kColors, state_.knobColor);
        },
        [this] {
            const std::string color = colorNode_->dataItems[colorNode_->dataPosition];
            if (p_.led) p_.led->setColor(color);
            state_.knobColor = color;
            menu_.changeMenuNodes();
        });

    brightnessNode_ = globalMenu_->addChild(
        "Knob Brightness",
        [this] {
            brightnessNode_->dataItems.clear();
            for (int b = 0; b <= 100; b += 10) brightnessNode_->dataItems.push_back(std::to_string(b));
            brightnessNode_->dataPrompt = "Knob Brightness:";
            brightnessNode_->dataPosition = indexOf(brightnessNode_->dataItems, std::to_string(state_.knobBrightness));
        },
        [this] {
            int b = std::stoi(brightnessNode_->dataItems[brightnessNode_->dataPosition]);
            if (p_.led) p_.led->setBrightness(b);
            state_.knobBrightness = b;
            menu_.changeMenuNodes();
        });

    lockNode_ = globalMenu_->addChild(
        "Button Lock",
        [this] {
            lockNode_->dataItems = {"True", "False"};
            lockNode_->dataPrompt = "Button Lock:";
            lockNode_->dataPosition = state_.buttonsLocked ? 0 : 1;
        },
        [this] {
            state_.buttonsLocked = (lockNode_->dataItems[lockNode_->dataPosition] == "True");
            menu_.changeMenuNodes();
        });
}

// Restores the Python on-device pedal editor (RotaryEncoder.show_midi_pedals and
// friends): Setup -> Midi Pedals -> [pedal] -> [group] -> [param] -> value. Edits
// are applied live through the existing MidiPedal methods (the same path loadPart
// uses); like the Python original they emit MIDI immediately and are not persisted
// back to the song file.
//
// CRITICAL: the tree is built LAZILY, one level per entry, NOT eagerly at boot. An
// eager build allocates ~160 nodes plus their closures during setup() — before the
// watchdog is armed and outside the boot try/catch — which on the MCU can throw
// bad_alloc / fragment the heap and brick powerup with a blank screen. The MenuTree
// engine already supports lazy build: a childless node carrying a `func` runs it on
// first entry (changeMenuNodes), so each `func` here populates its own children once
// (guarded by the empty() check) and the node then behaves as an ordinary branch.
// Boot therefore allocates exactly one node ("Midi Pedals").
void Application::buildPedalMenu() {
    midiPedalsMenu_ = setupMenu_->addChild("Midi Pedals");
    midiPedalsMenu_->func = [this] {
        if (!midiPedalsMenu_->children.empty()) return;  // build pedal list once
        for (const auto& pedalPtr : pedals_) {
            MidiPedal* mp = pedalPtr.get();
            auto cfgIt = pedalConfigs_.find(mp->name());
            if (cfgIt == pedalConfigs_.end()) continue;  // pedal built but config missing
            const PedalConfig* cfg = &cfgIt->second;

            MenuNode* pedalNode = midiPedalsMenu_->addChild(mp->name());
            pedalNode->func = [this, pedalNode, cfg, mp] { buildPedalGroups(pedalNode, cfg, mp); };
        }
    };
}

// Lazily builds one pedal's config-group children on first entry (Knobs/Switches,
// Parameters, Set Preset, Set Tempo, Engage, Bypass, Toggle Bypass), in config order.
void Application::buildPedalGroups(MenuNode* pedalNode, const PedalConfig* cfg, MidiPedal* mp) {
    if (!pedalNode->children.empty()) return;  // build once
    for (const auto& grpKv : cfg->root.children) {
        const std::string& groupName = grpKv.first;
        const Action* groupAction = &grpKv.second;

        if (groupName == "Knobs/Switches" || groupName == "Parameters") {
            // A branch whose leaves (one per knob/param) are built lazily on entry.
            MenuNode* groupNode = pedalNode->addChild(groupName);
            groupNode->func = [this, groupNode, groupAction, mp] {
                buildParamLeaves(groupNode, groupAction, mp);
            };
        } else if (groupName == "Set Preset" || groupName == "Set Tempo") {
            // A value-list leaf over the group's own min/max range.
            if (paramItems(*groupAction).empty()) continue;
            MenuNode* leaf = pedalNode->addChild(groupName);
            leaf->func = [leaf, groupAction, groupName] {
                leaf->dataItems = paramItems(*groupAction);
                leaf->dataPrompt = groupName + ":";
                leaf->dataPosition = 0;
            };
            leaf->dataFunc = [this, leaf, mp, groupName] {
                Value v{static_cast<long>(std::stol(leaf->dataItems[leaf->dataPosition]))};
                if (groupName == "Set Preset") mp->setPreset(v);
                else mp->setTempo(v);
                menu_.changeMenuNodes(leaf->parent);
            };
        } else if (groupName == "Engage" || groupName == "Bypass" || groupName == "Toggle Bypass") {
            // No value to choose: a NO/YES confirm leaf that fires the action (a leaf
            // needs a non-empty data list to be selectable at all).
            MenuNode* leaf = pedalNode->addChild(groupName);
            leaf->func = [leaf, groupName] {
                leaf->dataItems = {"NO", "YES"};
                leaf->dataPrompt = groupName + "?";
                leaf->dataPosition = 0;
            };
            leaf->dataFunc = [this, leaf, mp, groupName] {
                if (leaf->dataItems[leaf->dataPosition] == "YES") {
                    if (groupName == "Engage") mp->turnOn();
                    else if (groupName == "Bypass") mp->turnOff();
                    else mp->toggle();
                }
                menu_.changeMenuNodes(leaf->parent);
            };
        }
        // Unknown groups (e.g. display-only metadata) are skipped.
    }
}

// Lazily builds one Knobs/Switches or Parameters group's editable leaves on first
// entry — one per knob/param that exposes a choosable value.
void Application::buildParamLeaves(MenuNode* groupNode, const Action* groupAction, MidiPedal* mp) {
    if (!groupNode->children.empty()) return;  // build once
    for (const auto& paramKv : groupAction->children) {
        const std::string& paramName = paramKv.first;
        const Action* info = &paramKv.second;
        if (paramItems(*info).empty()) continue;  // nothing to choose -> skip (Python's `if value:`)

        MenuNode* leaf = groupNode->addChild(paramName);
        leaf->func = [leaf, info, paramName] {
            leaf->dataItems = paramItems(*info);
            leaf->dataPrompt = paramName + ":";
            leaf->dataPosition = 0;
        };
        leaf->dataFunc = [this, leaf, info, mp, paramName] {
            const std::string& item = leaf->dataItems[leaf->dataPosition];
            mp->setParams({{paramName, paramValue(*info, item)}});
            menu_.changeMenuNodes(leaf->parent);
        };
    }
}

}  // namespace mc
