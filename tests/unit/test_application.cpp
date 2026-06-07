// Application navigation, footswitch action mapping, global-menu edits, and
// button-lock behavior (finer-grained than the e2e flow tests).
#include "gtest/gtest.h"
#include "mc/adapters/sim/FsConfigStore.h"
#include "mc/adapters/sim/SimAdapters.h"
#include "mc/app/Application.h"
#include "support/FakeClock.h"
#include "support/RecordingMidiOut.h"
#include "support/RecordingPorts.h"

using namespace mc;
using namespace mc::test;

namespace {
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
    Rig() : app({&store, &midi, &tempo, &display, &led, &clock, &input, &transport}) { app.setup(); }

    void fsShort(int b) { app.handleEvent({InputEvent::Type::FootswitchShort, b, 0}); }
    void fsLong(int b) { app.handleEvent({InputEvent::Type::FootswitchLong, b, 0}); }
    void cw() { app.handleEvent({InputEvent::Type::EncoderCW, 0, 0}); }
    void ccw() { app.handleEvent({InputEvent::Type::EncoderCCW, 0, 0}); }
    void rotary(double s) { app.handleEvent({InputEvent::Type::RotaryPress, 0, s}); }
};
}  // namespace

// ---- preview navigation + bounds -------------------------------------------

TEST(AppNav, StartsOnConfiguredSongPart) {
    Rig r;
    EXPECT_EQ(r.app.currentSongName(), "Victory - Rhythm");
    EXPECT_EQ(r.app.currentPartName(), "Chorus");
    EXPECT_EQ(r.app.displayedPartName(), "Chorus");
}

TEST(AppNav, PrevPartAtStartIsClamped) {
    Rig r;
    r.app.prevPart();  // already at part 0
    EXPECT_EQ(r.app.displayedPartName(), "Chorus");
    EXPECT_TRUE(r.midi.messages.empty());
}

TEST(AppNav, NextPartPreviewsThenClampsAtEnd) {
    Rig r;
    r.app.nextPart();  // -> Bridge (Victory - Rhythm has 2 parts)
    EXPECT_EQ(r.app.displayedPartName(), "Bridge");
    r.app.nextPart();  // clamp
    EXPECT_EQ(r.app.displayedPartName(), "Bridge");
    EXPECT_TRUE(r.midi.messages.empty());  // preview never emits
}

TEST(AppNav, PrevSongAtStartClamped) {
    Rig r;
    r.app.prevSong();
    EXPECT_EQ(r.app.displayedSongName(), "Victory - Rhythm");
}

TEST(AppNav, NextSongResetsDisplayedPartToFirst) {
    Rig r;
    r.app.nextPart();  // displayed part -> Bridge
    r.app.nextSong();  // -> Here As In Heaven - Lead, part resets to Intro
    EXPECT_EQ(r.app.displayedSongName(), "Here As In Heaven - Lead");
    EXPECT_EQ(r.app.displayedPartName(), "Intro");
}

TEST(AppNav, SelectWithNoChangeEmitsNothing) {
    Rig r;
    r.app.selectChoice();  // current == displayed
    EXPECT_TRUE(r.midi.messages.empty());
}

TEST(AppNav, SelectAfterPreviewCommitsAndEmits) {
    Rig r;
    r.app.nextPart();      // preview Bridge
    r.app.selectChoice();  // commit
    EXPECT_EQ(r.app.currentPartName(), "Bridge");
    EXPECT_FALSE(r.midi.messages.empty());
    EXPECT_DOUBLE_EQ(r.tempo.bpms.back(), 110.0);
}

// ---- footswitch action mapping ---------------------------------------------

TEST(AppButtons, ShortPressMapping) {
    Rig r;
    r.fsShort(3);  // Part Up
    EXPECT_EQ(r.app.displayedPartName(), "Bridge");
    r.fsShort(1);  // Part Dn
    EXPECT_EQ(r.app.displayedPartName(), "Chorus");
    r.fsShort(5);  // Song Up
    EXPECT_EQ(r.app.displayedSongName(), "Here As In Heaven - Lead");
    r.fsShort(4);  // Song Dn
    EXPECT_EQ(r.app.displayedSongName(), "Victory - Rhythm");
}

TEST(AppButtons, SelectCommits) {
    Rig r;
    r.fsShort(3);  // preview Bridge
    r.fsShort(2);  // Select
    EXPECT_EQ(r.app.currentPartName(), "Bridge");
}

TEST(AppButtons, LongPressChangeAndSelectImmediately) {
    Rig r;
    r.fsLong(3);  // Select Part Up -> next + select
    EXPECT_EQ(r.app.currentPartName(), "Bridge");
    EXPECT_FALSE(r.midi.messages.empty());
}

TEST(AppButtons, LockedBlocksFootswitchButNotMenu) {
    Rig r;
    r.app.state().buttonsLocked = true;
    r.fsShort(3);
    EXPECT_EQ(r.app.displayedPartName(), "Chorus");  // unchanged
    r.fsLong(3);
    EXPECT_EQ(r.app.currentPartName(), "Chorus");
    r.rotary(0.2);  // menu still works
    EXPECT_EQ(r.display.last(), "Setup: - Sets");
}

// ---- global menu edits ------------------------------------------------------

TEST(AppMenu, KnobColorEdit) {
    Rig r;
    r.rotary(3.0);  // root -> Global (2..5s)
    EXPECT_EQ(r.display.last(), "Global: - Knob Color");
    r.rotary(0.2);  // enter Knob Color (position = current "Yellow")
    EXPECT_EQ(r.display.last(), "Global: - Knob Color: - Yellow");
    r.cw();  // -> White
    EXPECT_EQ(r.display.last(), "Global: - Knob Color: - White");
    r.rotary(0.2);  // select
    EXPECT_EQ(r.app.state().knobColor, "White");
    EXPECT_EQ(r.app.menu().current(), r.app.menu().root());  // back to root
}

TEST(AppMenu, ButtonLockEdit) {
    Rig r;
    r.rotary(3.0);  // Global
    r.cw();         // Knob Brightness
    r.cw();         // Button Lock
    EXPECT_EQ(r.display.last(), "Global: - Button Lock");
    r.rotary(0.2);  // enter; current false -> position "False"
    EXPECT_EQ(r.display.last(), "Global: - Button Lock: - False");
    r.ccw();        // -> True
    EXPECT_EQ(r.display.last(), "Global: - Button Lock: - True");
    r.rotary(0.2);  // select
    EXPECT_TRUE(r.app.state().buttonsLocked);
}

TEST(AppMenu, BrightnessEdit) {
    Rig r;
    r.rotary(3.0);  // Global
    r.cw();         // Knob Brightness
    r.rotary(0.2);  // enter; current 100 -> last item
    EXPECT_EQ(r.display.last(), "Global: - Knob Brightness: - 100");
    r.ccw();        // -> 90
    r.rotary(0.2);  // select
    EXPECT_EQ(r.app.state().knobBrightness, 90);
}

TEST(AppMenu, PowerOffQuits) {
    Rig r;
    r.rotary(6.0);  // -> Power, "Power Off? - NO"
    EXPECT_EQ(r.display.last(), "Power Off? - NO");
    r.cw();         // -> YES
    r.rotary(0.2);  // select YES
    EXPECT_TRUE(r.app.quitRequested());
}

TEST(AppMenu, SetsMenuListsFromStore) {
    Rig r;
    r.rotary(0.2);  // Setup
    r.rotary(0.2);  // enter Sets (3 set files on disk)
    EXPECT_EQ(r.display.last(), "Setup: - Sets: - 2017-05-22 Set");  // current set, sorted list
}
