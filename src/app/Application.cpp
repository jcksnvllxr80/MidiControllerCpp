#include "mc/app/Application.h"

#include <algorithm>

#include "mc/config/ConfigLoader.h"

namespace mc {

namespace {
int indexOf(const std::vector<std::string>& v, const std::string& s, int dflt = 0) {
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i] == s) return static_cast<int>(i);
    return dflt;
}
}  // namespace

Application::Application(Ports ports) : p_(ports) {}

void Application::setup() {
    state_ = config::loadControllerStateFromString(p_.store->read("midi_controller.json"));

    loadSetlist(state_.currentSet);
    currentSongIdx_ = std::max(0, setlist_.indexOfSong(state_.currentSong));
    currentPartIdx_ = std::max(0, currentSong().indexOfPart(state_.currentPart));
    displayedSongIdx_ = currentSongIdx_;
    displayedPartIdx_ = currentPartIdx_;

    // Build MidiPedals in channel order. The PedalConfig objects live in
    // pedalConfigs_ (std::map -> stable addresses), referenced by the pedals.
    for (const auto& ch : state_.channels) {
        auto it = pedalConfigs_.emplace(ch.name, config::loadPedalConfigFromString(
                                                     p_.store->read("pedals/" + ch.name + ".json")))
                      .first;
        pedals_.push_back(std::make_unique<MidiPedal>(ch.name, ch.channel, &it->second, p_.midi));
    }

    if (p_.led) {
        p_.led->setColor(state_.knobColor);
        p_.led->setBrightness(state_.knobBrightness);
    }
    buildMenu();
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
    if (p_.tempo) p_.tempo->setBpm(currentSong().bpmValue());
}

// --- display messages --------------------------------------------------------

std::string Application::songInfoString(const Song& s, const Part& p) const {
    return s.name + " - " + s.bpm + "BPM - " + p.name;
}
void Application::setSongInfoMessage() {
    if (p_.display) p_.display->setMessage(songInfoString(currentSong(), currentPart()));
}
void Application::previewMessage() {
    if (p_.display) p_.display->setMessage(songInfoString(displayedSong(), displayedPart()));
}

// --- menu wiring -------------------------------------------------------------

void Application::buildMenu() {
    menu_.setSink([this](const std::string& m) {
        if (p_.display) p_.display->setMessage(m);
    });
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

}  // namespace mc
