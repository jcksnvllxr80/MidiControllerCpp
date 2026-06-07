// Sim entry point. Boots the core from data/ and runs a scripted sequence of
// footswitch / encoder / rotary events, logging every MIDI message, tempo
// change, LED update and OLED line — the desktop stand-in for the real pedal.
#include <iostream>

#include "mc/adapters/sim/FsConfigStore.h"
#include "mc/adapters/sim/SimAdapters.h"
#include "mc/app/Application.h"

int main() {
    using namespace mc;

    sim::FsConfigStore store("data");
    sim::LoggingMidiOut midi;
    sim::LoggingTempoOut tempo;
    sim::ConsoleDisplay display;
    sim::LoggingLed led;
    sim::ChronoClock clock;
    sim::NullConfigTransport transport;

    // Deterministic demo script (no stdin needed).
    sim::ScriptedInput input;
    input.footswitchLong(3)    // "Select Part Up": jump to Bridge and load it
        .footswitchShort(1)    // "Part Dn": preview Chorus
        .footswitchShort(2)    // "Select": load Chorus
        .footswitchShort(5)    // "Song Up": preview next song
        .footswitchShort(2)    // "Select": load next song's first part
        .rotaryPress(0.2)      // rotary short press: enter Setup menu
        .encoderCW()           // Setup -> Songs
        .encoderCW()           // Setup -> Parts
        .rotaryPress(0.2)      // enter Parts list
        .encoderCW()           // highlight next part
        .rotaryPress(0.2);     // select it -> load that part

    Application app({&store, &midi, &tempo, &display, &led, &clock, &input, &transport});

    std::cout << "=== MidiController sim: boot from data/ ===\n";
    app.setup();
    std::cout << "=== running scripted input ===\n";
    app.run();
    std::cout << "=== done ===\n";
    return 0;
}
