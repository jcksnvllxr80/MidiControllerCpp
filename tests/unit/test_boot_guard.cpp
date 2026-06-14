// Boot must survive a corrupt config: Application::setup() catches a parse failure
// and falls back to a safe, still-reachable shell instead of throwing (which on the
// MCU would terminate / crash-loop at boot).
#include <filesystem>

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

TEST(BootGuard, MalformedConfigFallsBackInsteadOfThrowing) {
    fs::path base = fs::temp_directory_path() / "mc_boot_guard";
    fs::remove_all(base);
    fs::create_directories(base.string());
    sim::FsConfigStore store(base.string());
    store.write("midi_controller.json", "{ not valid json at all ");

    RecordingMidiOut midi;
    RecordingTempoOut tempo;
    RecordingDisplay display;
    NullLed led;
    FakeClock clock;
    sim::ScriptedInput input;
    sim::NullConfigTransport tr;
    Application app({&store, &midi, &tempo, &display, &led, &clock, &input, &tr});

    EXPECT_NO_THROW(app.setup());            // the whole point: no crash on bad config
    EXPECT_TRUE(app.setupFailed());
    ASSERT_FALSE(display.messages.empty());  // shows a config-error message to the user
    EXPECT_NE(display.last().find("Config"), std::string::npos);
    // ...and it's still driveable (no out_of_range from the empty setlist).
    EXPECT_NO_THROW(app.handleEvent({InputEvent::Type::EncoderCW, 0, 0.0}));

    fs::remove_all(base);
}
