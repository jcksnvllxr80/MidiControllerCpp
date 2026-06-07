// Application branches not exercised elsewhere: Songs/Sets menu selection,
// song-direction long press, Quit, encoder routing, cross-song select.
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
    void rotary(double s) { app.handleEvent({InputEvent::Type::RotaryPress, 0, s}); }
};
}  // namespace

TEST(AppMore, SongsMenuSelectionLoadsSong) {
    Rig r;
    r.rotary(0.2);  // Setup
    r.cw();         // -> Songs
    r.rotary(0.2);  // enter Songs (current song highlighted)
    EXPECT_EQ(r.display.last(), "Setup: - Songs: - Victory - Rhythm");
    r.cw();         // -> Here As In Heaven - Lead
    EXPECT_EQ(r.display.last(), "Setup: - Songs: - Here As In Heaven - Lead");
    r.midi.clear();
    r.rotary(0.2);  // select -> load that song's first part
    EXPECT_EQ(r.app.currentSongName(), "Here As In Heaven - Lead");
    EXPECT_EQ(r.app.currentPartName(), "Intro");
    EXPECT_FALSE(r.midi.messages.empty());
    EXPECT_EQ(r.app.menu().current(), r.app.menu().root());
}

TEST(AppMore, SetsMenuSelectionSwitchesSetlist) {
    Rig r;
    r.rotary(0.2);  // Setup
    r.rotary(0.2);  // enter Sets (current "2017-05-22 Set" highlighted)
    EXPECT_EQ(r.display.last(), "Setup: - Sets: - 2017-05-22 Set");
    r.app.handleEvent({InputEvent::Type::EncoderCCW, 0, 0});  // -> 2016-09-16 Set
    EXPECT_EQ(r.display.last(), "Setup: - Sets: - 2016-09-16 Set");
    r.midi.clear();
    r.rotary(0.2);  // select
    EXPECT_EQ(r.app.state().currentSet, "2016-09-16 Set");
    EXPECT_EQ(r.app.setlist().name, "2016-09-16 Set");
    EXPECT_EQ(r.app.currentSongName(), "Victory - Rhythm");  // first song of that set
    EXPECT_FALSE(r.midi.messages.empty());                   // loaded the part
}

TEST(AppMore, LongPressSelectSongUpCommits) {
    Rig r;
    r.fsLong(5);  // button 5 long = "Select Song Up"
    EXPECT_EQ(r.app.currentSongName(), "Here As In Heaven - Lead");
    EXPECT_EQ(r.app.currentPartName(), "Intro");
}

TEST(AppMore, LongPressSelectSongDownCommits) {
    Rig r;
    r.fsLong(5);  // up to song 1
    EXPECT_EQ(r.app.currentSongName(), "Here As In Heaven - Lead");
    r.fsLong(4);  // button 4 long = "Select Song Dn" -> back to song 0
    EXPECT_EQ(r.app.currentSongName(), "Victory - Rhythm");
}

TEST(AppMore, SelectCommitsCrossSong) {
    Rig r;
    r.fsShort(5);  // preview next song
    r.fsShort(5);  // preview song 2 (Victory - Lead)
    EXPECT_EQ(r.app.displayedSongName(), "Victory - Lead");
    EXPECT_EQ(r.app.currentSongName(), "Victory - Rhythm");  // not committed
    r.fsShort(2);  // Select -> loadSong
    EXPECT_EQ(r.app.currentSongName(), "Victory - Lead");
}

TEST(AppMore, HandleEventQuitReturnsFalse) {
    Rig r;
    EXPECT_FALSE(r.app.handleEvent({InputEvent::Type::Quit, 0, 0}));
    EXPECT_TRUE(r.app.handleEvent({InputEvent::Type::FootswitchShort, 3, 0}));  // others keep looping
}

TEST(AppMore, EncoderRoutesToMenu) {
    Rig r;
    r.rotary(0.2);  // Setup, child 0 (Sets)
    r.cw();         // EncoderCW -> child 1 (Songs)
    EXPECT_EQ(r.display.last(), "Setup: - Songs");
    r.app.handleEvent({InputEvent::Type::EncoderCCW, 0, 0});  // back to Sets
    EXPECT_EQ(r.display.last(), "Setup: - Sets");
}

TEST(AppMore, NextSongClampsAtLastSong) {
    Rig r;  // setlist has 4 songs
    for (int i = 0; i < 6; ++i) r.fsShort(5);  // Song Up many times
    EXPECT_EQ(r.app.displayedSongName(), "Here As In Heaven - Rhythm");  // last
}
