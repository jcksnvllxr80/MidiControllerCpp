// Sim adapters: logging output, scripted input ordering/exhaustion, clock.
#include <sstream>

#include "gtest/gtest.h"
#include "mc/adapters/sim/SimAdapters.h"

using namespace mc;

TEST(SimMidi, LogsByteString) {
    std::ostringstream os;
    sim::LoggingMidiOut midi(os);
    midi.send(MidiMessage::controlChange(2, 102, 127));
    EXPECT_NE(os.str().find("B1 66 7F"), std::string::npos);
    EXPECT_NE(os.str().find("midi"), std::string::npos);
}

TEST(SimDisplay, RecordsAndLogs) {
    std::ostringstream os;
    sim::ConsoleDisplay d(os);
    d.setMessage("Hello");
    EXPECT_EQ(d.last(), "Hello");
    EXPECT_NE(os.str().find("Hello"), std::string::npos);
    d.clear();
    EXPECT_NE(os.str().find("clear"), std::string::npos);
}

TEST(SimTempo, LogsBpmAndTap) {
    std::ostringstream os;
    sim::LoggingTempoOut t(os);
    t.setBpm(110.0);
    t.tap();
    EXPECT_NE(os.str().find("110"), std::string::npos);
    EXPECT_NE(os.str().find("tap"), std::string::npos);
}

TEST(SimLed, LogsColorAndBrightness) {
    std::ostringstream os;
    sim::LoggingLed led(os);
    led.setColor("Yellow");
    led.setBrightness(80);
    EXPECT_NE(os.str().find("Yellow"), std::string::npos);
    EXPECT_NE(os.str().find("80"), std::string::npos);
}

TEST(SimInput, DeliversInOrderThenExhausts) {
    sim::ScriptedInput in;
    in.footswitchShort(1).footswitchLong(2).encoderCW().encoderCCW().rotaryPress(0.3).quit();
    InputEvent e;
    ASSERT_TRUE(in.poll(e));
    EXPECT_EQ(e.type, InputEvent::Type::FootswitchShort);
    EXPECT_EQ(e.button, 1);
    ASSERT_TRUE(in.poll(e));
    EXPECT_EQ(e.type, InputEvent::Type::FootswitchLong);
    EXPECT_EQ(e.button, 2);
    ASSERT_TRUE(in.poll(e));
    EXPECT_EQ(e.type, InputEvent::Type::EncoderCW);
    ASSERT_TRUE(in.poll(e));
    EXPECT_EQ(e.type, InputEvent::Type::EncoderCCW);
    ASSERT_TRUE(in.poll(e));
    EXPECT_EQ(e.type, InputEvent::Type::RotaryPress);
    EXPECT_DOUBLE_EQ(e.holdSeconds, 0.3);
    ASSERT_TRUE(in.poll(e));
    EXPECT_EQ(e.type, InputEvent::Type::Quit);
    EXPECT_FALSE(in.poll(e));  // exhausted
}

TEST(SimInput, EmptyPollReturnsFalse) {
    sim::ScriptedInput in;
    InputEvent e;
    EXPECT_FALSE(in.poll(e));
}

TEST(SimClock, NonDecreasing) {
    sim::ChronoClock c;
    double a = c.now();
    double b = c.now();
    EXPECT_GE(b, a);
    EXPECT_GT(a, 0.0);
}

TEST(SimTransport, NullIsNoop) {
    sim::NullConfigTransport t;
    t.begin();
    t.poll();  // must not crash
    SUCCEED();
}
