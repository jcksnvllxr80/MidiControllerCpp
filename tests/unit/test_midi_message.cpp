#include "mc/domain/MidiMessage.h"

#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"

using namespace mc;
using Bytes = std::vector<uint8_t>;

TEST(MidiMessage, ControlChangeChannel1) {
    auto m = MidiMessage::controlChange(1, 102, 127);
    EXPECT_EQ(m.type(), MidiMessage::Type::ControlChange);
    EXPECT_EQ(m.bytes(), (Bytes{0xB0, 102, 127}));
}

TEST(MidiMessage, ControlChangeChannel2) {
    // TimeLine Engage: ch2, cc102, value127.
    auto m = MidiMessage::controlChange(2, 102, 127);
    EXPECT_EQ(m.bytes(), (Bytes{0xB1, 0x66, 0x7F}));
}

TEST(MidiMessage, ControlChangeChannel16) {
    // ScarlettLove (ch16) selecting Plexi -> CC#21 value 127.
    auto m = MidiMessage::controlChange(16, 21, 127);
    EXPECT_EQ(m.bytes(), (Bytes{0xBF, 0x15, 0x7F}));
}

TEST(MidiMessage, ProgramChange) {
    // TimeLine Set Preset -> PC on ch2.
    auto m = MidiMessage::programChange(2, 1);
    EXPECT_EQ(m.type(), MidiMessage::Type::ProgramChange);
    EXPECT_EQ(m.bytes(), (Bytes{0xC1, 0x01}));
}

TEST(MidiMessage, BypassIsValueZero) {
    auto m = MidiMessage::controlChange(2, 102, 0);
    EXPECT_EQ(m.bytes(), (Bytes{0xB1, 0x66, 0x00}));
}

TEST(MidiMessage, InvalidChannelThrows) {
    EXPECT_THROW(MidiMessage::controlChange(0, 1, 1), std::out_of_range);
    EXPECT_THROW(MidiMessage::controlChange(17, 1, 1), std::out_of_range);
    EXPECT_THROW(MidiMessage::programChange(0, 1), std::out_of_range);
}

TEST(MidiMessage, Equality) {
    EXPECT_EQ(MidiMessage::controlChange(3, 10, 20), MidiMessage::controlChange(3, 10, 20));
    EXPECT_NE(MidiMessage::controlChange(3, 10, 20), MidiMessage::controlChange(3, 10, 21));
    EXPECT_NE(MidiMessage::controlChange(3, 10, 20), MidiMessage::programChange(3, 10));
}
