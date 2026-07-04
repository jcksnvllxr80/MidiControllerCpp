// On-device pedal editor (Setup -> Midi Pedals -> pedal -> group -> param).
// Drives the rotary encoder/push-button exactly as the hardware would and asserts
// the live MIDI a value edit emits, mirroring the Python RotaryEncoder pedal menu.
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
    RecordingDisplay display;
    NullLed led;
    FakeClock clock;
    sim::ScriptedInput input;
    sim::NullConfigTransport transport;
    Application app;
    Rig() : app({&store, &midi, &display, &led, &clock, &input, &transport}) { app.setup(); }

    void cw() { app.handleEvent({InputEvent::Type::EncoderCW, 0, 0}); }
    void rotary(double s) { app.handleEvent({InputEvent::Type::RotaryPress, 0, s}); }
};
}  // namespace

// Full navigation: root -> Setup -> Midi Pedals -> TimeLine -> Knobs/Switches ->
// Type -> "Digital". TimeLine is channel 2; Type is cc 19 with dict Digital=2, so
// the commit must emit CC ch=2 cc=19 val=2 and return to the Knobs/Switches branch.
TEST(AppPedalMenu, EditKnobEmitsLiveCc) {
    Rig r;
    r.rotary(0.2);                                     // root -> Setup (shows "Setup: - Sets")
    r.cw(); r.cw(); r.cw();                            // -> Midi Pedals (Sets/Songs/Parts/Midi Pedals)
    EXPECT_EQ(r.display.last(), "Setup: - Midi Pedals");
    r.rotary(0.2);                                     // enter Midi Pedals (pedals in channel order)
    EXPECT_EQ(r.display.last(), "Setup: - Midi Pedals: - QuartzV2");
    r.cw();                                            // -> TimeLine (channel 2)
    EXPECT_EQ(r.display.last(), "Setup: - Midi Pedals: - TimeLine");
    r.rotary(0.2);                                     // enter TimeLine (group list)
    EXPECT_EQ(r.display.last(), "Midi Pedals: - TimeLine: - Knobs/Switches");
    r.rotary(0.2);                                     // enter Knobs/Switches (param list)
    EXPECT_EQ(r.display.last(), "TimeLine: - Knobs/Switches: - Type");
    r.rotary(0.2);                                     // enter Type (value list, pos0 = dTape)
    EXPECT_EQ(r.display.last(), "TimeLine: - Knobs/Switches: - Type: - dTape");

    ASSERT_TRUE(r.midi.messages.empty());              // navigation never emits MIDI
    r.cw(); r.cw();                                    // dTape(0) -> dBucket(1) -> Digital(2)
    EXPECT_EQ(r.display.last(), "TimeLine: - Knobs/Switches: - Type: - Digital");
    r.rotary(0.2);                                     // select -> emit + back to Knobs/Switches

    ASSERT_EQ(r.midi.messages.size(), 1u);
    EXPECT_EQ(r.midi.messages[0], MidiMessage::controlChange(2, 19, 2));
    EXPECT_EQ(r.display.last(), "TimeLine: - Knobs/Switches: - Type");  // returned to parent
}

// Engage is an action group (no value to choose): a NO/YES confirm leaf that fires
// the pedal's Engage action only on YES. TimeLine Engage is cc 31 value 127.
TEST(AppPedalMenu, EngageConfirmFires) {
    Rig r;
    r.rotary(0.2);                  // Setup
    r.cw(); r.cw(); r.cw();         // Midi Pedals
    r.rotary(0.2);                  // enter (QuartzV2)
    r.cw();                         // TimeLine
    r.rotary(0.2);                  // enter TimeLine groups (Knobs/Switches)
    r.cw(); r.cw(); r.cw();         // -> Engage (Knobs/Switches, Parameters, Set Preset, Engage)
    EXPECT_EQ(r.display.last(), "Midi Pedals: - TimeLine: - Engage");
    r.rotary(0.2);                  // enter Engage confirm (pos0 = NO)
    EXPECT_EQ(r.display.last(), "Midi Pedals: - TimeLine: - Engage? - NO");
    r.cw();                         // -> YES
    r.rotary(0.2);                  // confirm -> fires Engage

    ASSERT_EQ(r.midi.messages.size(), 1u);
    EXPECT_EQ(r.midi.messages[0].channel(), 2);
}
