// "Save defaults": a committed song/part change is written back to
// midi_controller.json, debounced (no write until an idle gap), and survives a
// reboot. Runs against a temp copy of data/ so the committed fixtures stay clean.
#include <filesystem>

#include <nlohmann/json.hpp>

#include "gtest/gtest.h"
#include "mc/adapters/sim/FsConfigStore.h"
#include "mc/adapters/sim/SimAdapters.h"
#include "mc/app/Application.h"
#include "support/FakeClock.h"
#include "support/RecordingMidiOut.h"
#include "support/RecordingPorts.h"

namespace fs = std::filesystem;
using namespace mc;
using namespace mc::test;
using json = nlohmann::json;

namespace {
struct Rig {
    sim::FsConfigStore store;
    RecordingMidiOut midi;
    RecordingTempoOut tempo;
    RecordingDisplay display;
    NullLed led;
    FakeClock clock;
    sim::ScriptedInput input;
    sim::NullConfigTransport tr;
    Application app;
    explicit Rig(const std::string& dir)
        : store(dir), app({&store, &midi, &tempo, &display, &led, &clock, &input, &tr}) {
        app.setup();
    }
    std::string savedSong() { return json::parse(store.read("midi_controller.json"))["current_settings"]["preset"]["song"]; }
};
}  // namespace

TEST(SaveDefaults, CommittedSongPersistsDebouncedAndSurvivesReboot) {
    fs::path base = fs::temp_directory_path() / "mc_save_defaults";
    fs::remove_all(base);
    fs::copy("data", base, fs::copy_options::recursive);

    std::string committed;
    {
        Rig r(base.string());
        const std::string boot = r.app.currentSongName();

        // Commit a song different from the boot song: clamp to song 0 and commit;
        // if that *was* the boot song, step to song 1 and commit instead.
        r.app.prevSong();
        r.app.prevSong();
        r.app.prevSong();
        r.app.selectChoice();
        if (r.app.currentSongName() == boot) {
            r.app.nextSong();
            r.app.selectChoice();
        }
        committed = r.app.currentSongName();
        ASSERT_NE(committed, boot);

        // Debounced: still the boot value right after the change (tick at t=0).
        r.app.tick();
        EXPECT_EQ(r.savedSong(), boot);

        // ...flushed once the idle gap has elapsed.
        r.clock.advance(2.0);
        r.app.tick();
        EXPECT_EQ(r.savedSong(), committed);
    }

    // A fresh boot from the same store comes up on the persisted song.
    Rig r2(base.string());
    EXPECT_EQ(r2.app.currentSongName(), committed);

    fs::remove_all(base);
}
