#pragma once
//
// Application — the composition root and event loop. Ports the setup()/main()
// glue from midi_controller.py plus the song/set/part navigation from
// RotaryEncoder.py:
//   * builds MidiPedals from the channel table (in channel order)
//   * footswitches preview (displayed) song/part; "Select" commits (loads MIDI)
//   * long-press footswitches commit immediately (change_and_select)
//   * the rotary knob drives the MenuTree (setup/global/power)
//
// All hardware is behind ports, so the same Application runs on the sim now and
// the microcontroller later.
//
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mc/domain/ControllerState.h"
#include "mc/domain/MenuTree.h"
#include "mc/domain/MidiPedal.h"
#include "mc/domain/PedalConfig.h"
#include "mc/domain/SongPartSet.h"
#include "mc/ports/IClock.h"
#include "mc/ports/IConfigStore.h"
#include "mc/ports/IConfigTransport.h"
#include "mc/ports/IDisplay.h"
#include "mc/ports/IInput.h"
#include "mc/ports/ILed.h"
#include "mc/ports/IMidiOut.h"
#include "mc/ports/ITempoOut.h"

namespace mc {

class Application {
public:
    struct Ports {
        IConfigStore* store = nullptr;
        IMidiOut* midi = nullptr;
        ITempoOut* tempo = nullptr;
        IDisplay* display = nullptr;
        ILed* led = nullptr;
        IClock* clock = nullptr;
        IInput* input = nullptr;
        IConfigTransport* transport = nullptr;  // optional
    };

    explicit Application(Ports ports);

    void setup();        // load config, build pedals/menu, set initial display
    void run();          // poll input -> dispatch until Quit / source exhausted
    bool handleEvent(const InputEvent& ev);  // false => stop loop

    // --- inspectors (for tests) ---
    ControllerState& state() { return state_; }
    Setlist& setlist() { return setlist_; }
    MenuTree& menu() { return menu_; }
    const std::string& currentSongName() const { return currentSong().name; }
    const std::string& currentPartName() const { return currentPart().name; }
    const std::string& displayedSongName() const { return displayedSong().name; }
    const std::string& displayedPartName() const { return displayedPart().name; }
    const std::string& displayedMessage() const { return lastMessage_; }  // last text shown
    bool quitRequested() const { return quitRequested_; }

    // Navigation, exposed so tests/menus can drive them directly.
    void loadPart();
    void loadSong();
    void selectChoice();
    void nextSong();
    void prevSong();
    void nextPart();
    void prevPart();

private:
    const Song& currentSong() const { return setlist_.songs.at(currentSongIdx_); }
    const Part& currentPart() const { return currentSong().parts.at(currentPartIdx_); }
    const Song& displayedSong() const { return setlist_.songs.at(displayedSongIdx_); }
    const Part& displayedPart() const { return displayedSong().parts.at(displayedPartIdx_); }

    void loadSetlist(const std::string& setName);
    void buttonExecutor(const std::string& fn);
    void changeAndSelect(const std::string& fn);
    void setSongInfoMessage();
    void previewMessage();
    void show(const std::string& msg);  // route all display text through here
    std::string songInfoString(const Song& s, const Part& p) const;
    void buildMenu();

    Ports p_;
    ControllerState state_;
    Setlist setlist_;
    std::map<std::string, PedalConfig> pedalConfigs_;
    std::vector<std::unique_ptr<MidiPedal>> pedals_;  // channel order

    int currentSongIdx_ = 0;
    int currentPartIdx_ = 0;
    int displayedSongIdx_ = 0;
    int displayedPartIdx_ = 0;
    bool quitRequested_ = false;
    std::string lastMessage_;

    MenuTree menu_{"MidiController"};
    MenuNode* setupMenu_ = nullptr;
    MenuNode* globalMenu_ = nullptr;
    MenuNode* powerMenu_ = nullptr;
    MenuNode* setsNode_ = nullptr;
    MenuNode* songsNode_ = nullptr;
    MenuNode* partsNode_ = nullptr;
    MenuNode* colorNode_ = nullptr;
    MenuNode* brightnessNode_ = nullptr;
    MenuNode* lockNode_ = nullptr;
};

}  // namespace mc
