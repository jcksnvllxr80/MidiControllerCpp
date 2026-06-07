// Data-byte handling: the constructors cast to uint8_t (no clamping). Real MIDI
// data is 0..127, but documenting the cast guards against silent surprises.
#include <vector>

#include "gtest/gtest.h"
#include "mc/domain/MidiMessage.h"

using namespace mc;
using Bytes = std::vector<uint8_t>;

TEST(MidiMessageBytes, ValuesUpTo255PassThrough) {
    EXPECT_EQ(MidiMessage::controlChange(1, 0, 200).bytes(), (Bytes{0xB0, 0x00, 200}));
    EXPECT_EQ(MidiMessage::controlChange(1, 200, 5).bytes(), (Bytes{0xB0, 200, 0x05}));
    EXPECT_EQ(MidiMessage::programChange(1, 200).bytes(), (Bytes{0xC0, 200}));
}

TEST(MidiMessageBytes, Over255WrapsViaUint8Cast) {
    EXPECT_EQ(MidiMessage::controlChange(1, 0, 256).bytes()[2], 0);    // 256 & 0xFF
    EXPECT_EQ(MidiMessage::controlChange(1, 0, 257).bytes()[2], 1);
    EXPECT_EQ(MidiMessage::programChange(1, 256).bytes()[1], 0);
}

TEST(MidiMessageBytes, ChannelStoredSeparatelyFromBytes) {
    auto m = MidiMessage::controlChange(10, 1, 1);
    EXPECT_EQ(m.channel(), 10);
    EXPECT_EQ(m.bytes()[0], 0xB9);  // 0xB0 | 9
}
