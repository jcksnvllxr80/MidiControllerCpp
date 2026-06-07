// End-to-end: drive the whole Application with scripted input, a fake clock and
// the *real converted fixtures* (data/), asserting the MIDI/tempo/display the
// rig would have produced. This is the in-process equivalent of the old Flask
// short/long/dpad endpoints.
#include <vector>

#include "gtest/gtest.h"
#include "mc/adapters/sim/FsConfigStore.h"
#include "mc/adapters/sim/SimAdapters.h"
#include "mc/app/Application.h"
#include "support/FakeClock.h"
#include "support/RecordingMidiOut.h"
#include "support/RecordingPorts.h"

using namespace mc;
using namespace mc::test;
using Bytes = std::vector<uint8_t>;
using Seq = std::vector<std::vector<uint8_t>>;

namespace {

// The exact byte sequence for loading Victory - Rhythm's Chorus/Bridge (identical
// pedal states), processed in channel order: QuartzV2(1) TimeLine(2) Iridium(3)
// BigSky(4) SuperSwitcher(5) ScarlettLove(16).
const Seq kVictoryChorus = {
    {0xB0, 0x61, 0x00},        // QuartzV2 preset 0 (cc97)
    {0xB1, 0x66, 0x00},        // TimeLine bypass (cc102=0)
    {0xC1, 0x02},              // TimeLine preset 2 (PC, bank cc:0 skipped)
    {0xB2, 0x66, 0x7F},        // Iridium engage
    {0xC2, 0x01},              // Iridium preset 1
    {0xB3, 0x66, 0x7F},        // BigSky engage
    {0xC3, 0x01},              // BigSky preset 1
    {0xC4, 0x01},              // SuperSwitcher preset 1 (PC)
    {0xB4, 0x66, 0x00},        // SuperSwitcher Loop 1 off
    {0xB4, 0x67, 0x00},        // Loop 2 off
    {0xB4, 0x68, 0x00},        // Loop 3 off
    {0xB4, 0x69, 0x00},        // Loop 4 off
    {0xB4, 0x6B, 0x00},        // Loop 6 off
    {0xB4, 0x6D, 0x00},        // Loop 7 off
    {0xB4, 0x6C, 0x00},        // Loop 8 off
    {0xB4, 0x70, 0x00},        // Control 1 off
    {0xB4, 0x71, 0x00},        // Control 2 off
    {0xBF, 0x17, 0x01},        // ScarlettLove engage (cc23)
    {0xBF, 0x15, 0x7F},        // ScarlettLove preset Plexi (control change -> cc21=127)
};

struct Rig {
    sim::FsConfigStore store{"data"};
    RecordingMidiOut midi;
    RecordingTempoOut tempo;
    RecordingDisplay display;
    NullLed led;
    FakeClock clock;
    sim::ScriptedInput input;
    sim::NullConfigTransport transport;
    Application app;

    Rig()
        : app({&store, &midi, &tempo, &display, &led, &clock, &input, &transport}) {}

    void fsShort(int b) { app.handleEvent({InputEvent::Type::FootswitchShort, b, 0}); }
    void fsLong(int b) { app.handleEvent({InputEvent::Type::FootswitchLong, b, 0}); }
    void cw() { app.handleEvent({InputEvent::Type::EncoderCW, 0, 0}); }
    void rotary(double s) { app.handleEvent({InputEvent::Type::RotaryPress, 0, s}); }
};

}  // namespace

TEST(E2E, BootShowsCurrentSongNoMidi) {
    Rig r;
    r.app.setup();
    EXPECT_EQ(r.display.last(), "Victory - Rhythm - 110BPM - Chorus");
    EXPECT_TRUE(r.midi.messages.empty());  // standard mode: no MIDI at boot
    EXPECT_EQ(r.app.currentPartName(), "Chorus");
}

TEST(E2E, LoadPartEmitsExactGoldenSequence) {
    Rig r;
    r.app.setup();
    r.app.loadPart();  // load current part (Chorus)
    EXPECT_EQ(r.midi.byteSeq(), kVictoryChorus);
    ASSERT_EQ(r.tempo.bpms.size(), 1u);
    EXPECT_DOUBLE_EQ(r.tempo.bpms.back(), 110.0);
}

TEST(E2E, FootswitchPreviewThenSelectCommits) {
    Rig r;
    r.app.setup();
    r.fsShort(3);  // "Part Up" -> preview Bridge (no MIDI)
    EXPECT_EQ(r.display.last(), "Victory - Rhythm - 110BPM - Bridge");
    EXPECT_TRUE(r.midi.messages.empty());
    EXPECT_EQ(r.app.currentPartName(), "Chorus");  // not committed yet

    r.fsShort(2);  // "Select" -> commit Bridge
    EXPECT_EQ(r.app.currentPartName(), "Bridge");
    EXPECT_EQ(r.midi.byteSeq(), kVictoryChorus);  // Bridge == Chorus bytes
    EXPECT_EQ(r.display.last(), "Victory - Rhythm - 110BPM - Bridge");
}

TEST(E2E, LongPressCommitsImmediately) {
    Rig r;
    r.app.setup();
    r.fsLong(3);  // "Select Part Up" -> nextPart + select in one shot
    EXPECT_EQ(r.app.currentPartName(), "Bridge");
    EXPECT_EQ(r.midi.byteSeq(), kVictoryChorus);
}

TEST(E2E, NextSongSelectLoadsNewSongFirstPartAndTempo) {
    Rig r;
    r.app.setup();
    r.fsShort(5);  // "Song Up" -> preview Here As In Heaven - Lead / Intro
    EXPECT_EQ(r.display.last(), "Here As In Heaven - Lead - 74BPM - Intro");
    EXPECT_TRUE(r.midi.messages.empty());

    r.fsShort(2);  // "Select" -> load it
    EXPECT_EQ(r.app.currentSongName(), "Here As In Heaven - Lead");
    EXPECT_EQ(r.app.currentPartName(), "Intro");
    EXPECT_DOUBLE_EQ(r.tempo.bpms.back(), 74.0);
    ASSERT_FALSE(r.midi.messages.empty());
    // QuartzV2 preset 1 first (channel order), ScarlettLove preset TS808 -> cc20=127.
    EXPECT_EQ(r.midi.messages.front().bytes(), (Bytes{0xB0, 0x61, 0x01}));
    EXPECT_EQ(r.midi.messages.back().bytes(), (Bytes{0xBF, 0x14, 0x7F}));
}

TEST(E2E, RotaryMenuEditSelectsPart) {
    Rig r;
    r.app.setup();
    r.rotary(0.2);  // short press at root -> Setup
    EXPECT_EQ(r.display.last(), "Setup: - Sets");
    r.cw();  // -> Songs
    r.cw();  // -> Parts
    EXPECT_EQ(r.display.last(), "Setup: - Parts");
    r.rotary(0.2);  // enter Parts list (current song parts)
    EXPECT_EQ(r.display.last(), "Setup: - Parts: - Chorus");
    r.cw();  // highlight Bridge
    EXPECT_EQ(r.display.last(), "Setup: - Parts: - Bridge");

    r.midi.clear();
    r.rotary(0.2);  // select Bridge -> load + back to root
    EXPECT_EQ(r.app.currentPartName(), "Bridge");
    EXPECT_EQ(r.midi.byteSeq(), kVictoryChorus);
    EXPECT_EQ(r.display.last(), "Victory - Rhythm - 110BPM - Bridge");
}

TEST(E2E, ButtonsLockedBlockFootswitchesButNotMenu) {
    Rig r;
    r.app.setup();
    r.app.state().buttonsLocked = true;
    r.fsShort(3);  // blocked
    EXPECT_EQ(r.app.displayedPartName(), "Chorus");  // no preview change
    EXPECT_TRUE(r.midi.messages.empty());

    r.rotary(0.2);  // menu still works when locked
    EXPECT_EQ(r.display.last(), "Setup: - Sets");
}

TEST(E2E, RunLoopProcessesScriptThenPowerOffQuits) {
    Rig r;
    // Hold > 5s -> Power menu, scroll to YES, select -> quit.
    r.input.rotaryPress(6.0).encoderCW().rotaryPress(0.2);
    r.app.setup();
    r.app.run();
    EXPECT_TRUE(r.app.quitRequested());
}

TEST(E2E, RunLoopEndsWhenInputExhausted) {
    Rig r;
    r.input.footswitchLong(3);  // load Bridge, then input runs out
    r.app.setup();
    r.app.run();
    EXPECT_EQ(r.app.currentPartName(), "Bridge");
    EXPECT_FALSE(r.app.quitRequested());
}
