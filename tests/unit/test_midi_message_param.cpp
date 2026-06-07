// Parametrized MidiMessage coverage: every channel, plus cc/value sweeps.
#include <vector>

#include "gtest/gtest.h"
#include "mc/domain/MidiMessage.h"

using namespace mc;
using Bytes = std::vector<uint8_t>;

class CcChannel : public ::testing::TestWithParam<int> {};
TEST_P(CcChannel, StatusAndShape) {
    int ch = GetParam();
    auto m = MidiMessage::controlChange(ch, 64, 100);
    ASSERT_EQ(m.bytes().size(), 3u);
    EXPECT_EQ(m.bytes()[0], static_cast<uint8_t>(0xB0 | (ch - 1)));
    EXPECT_EQ(m.bytes()[1], 64);
    EXPECT_EQ(m.bytes()[2], 100);
    EXPECT_EQ(m.channel(), ch);
    EXPECT_EQ(m.type(), MidiMessage::Type::ControlChange);
}
INSTANTIATE_TEST_SUITE_P(AllChannels, CcChannel, ::testing::Range(1, 17));

class PcChannel : public ::testing::TestWithParam<int> {};
TEST_P(PcChannel, StatusAndShape) {
    int ch = GetParam();
    auto m = MidiMessage::programChange(ch, 42);
    ASSERT_EQ(m.bytes().size(), 2u);
    EXPECT_EQ(m.bytes()[0], static_cast<uint8_t>(0xC0 | (ch - 1)));
    EXPECT_EQ(m.bytes()[1], 42);
    EXPECT_EQ(m.channel(), ch);
    EXPECT_EQ(m.type(), MidiMessage::Type::ProgramChange);
}
INSTANTIATE_TEST_SUITE_P(AllChannels, PcChannel, ::testing::Range(1, 17));

struct ByteCase {
    int ch, num, val;
    Bytes expected;
    std::string id;
};
class CcBytes : public ::testing::TestWithParam<ByteCase> {};
TEST_P(CcBytes, Exact) {
    const auto& c = GetParam();
    EXPECT_EQ(MidiMessage::controlChange(c.ch, c.num, c.val).bytes(), c.expected);
}
INSTANTIATE_TEST_SUITE_P(
    Sweep, CcBytes,
    ::testing::Values(ByteCase{1, 0, 0, {0xB0, 0x00, 0x00}, "min_all"},
                      ByteCase{1, 127, 127, {0xB0, 0x7F, 0x7F}, "max_all"},
                      ByteCase{2, 102, 64, {0xB1, 0x66, 0x40}, "mid"},
                      ByteCase{16, 21, 127, {0xBF, 0x15, 0x7F}, "scarlett"},
                      ByteCase{5, 113, 0, {0xB4, 0x71, 0x00}, "superswitcher_ctrl2"},
                      ByteCase{1, 1, 63, {0xB0, 0x01, 0x3F}, "low"},
                      ByteCase{8, 50, 50, {0xB7, 0x32, 0x32}, "ch8"}),
    [](const ::testing::TestParamInfo<ByteCase>& i) { return i.param.id; });

struct PcCase {
    int ch, prog;
    Bytes expected;
    std::string id;
};
class PcBytes : public ::testing::TestWithParam<PcCase> {};
TEST_P(PcBytes, Exact) {
    const auto& c = GetParam();
    EXPECT_EQ(MidiMessage::programChange(c.ch, c.prog).bytes(), c.expected);
}
INSTANTIATE_TEST_SUITE_P(Sweep, PcBytes,
                         ::testing::Values(PcCase{1, 0, {0xC0, 0x00}, "ch1_p0"},
                                           PcCase{2, 2, {0xC1, 0x02}, "ch2_p2"},
                                           PcCase{4, 72, {0xC3, 0x48}, "ch4_p72"},
                                           PcCase{5, 127, {0xC4, 0x7F}, "ch5_max"},
                                           PcCase{16, 1, {0xCF, 0x01}, "ch16"}),
                         [](const ::testing::TestParamInfo<PcCase>& i) { return i.param.id; });

class BadChannel : public ::testing::TestWithParam<int> {};
TEST_P(BadChannel, CcThrows) { EXPECT_THROW(MidiMessage::controlChange(GetParam(), 1, 1), std::out_of_range); }
TEST_P(BadChannel, PcThrows) { EXPECT_THROW(MidiMessage::programChange(GetParam(), 1), std::out_of_range); }
INSTANTIATE_TEST_SUITE_P(OutOfRange, BadChannel, ::testing::Values(0, -1, 17, 100, 255));

TEST(MidiMessageToString, ControlChangeFormat) {
    auto s = MidiMessage::controlChange(2, 102, 127).toString();
    EXPECT_NE(s.find("CC"), std::string::npos);
    EXPECT_NE(s.find("B1 66 7F"), std::string::npos);
}
TEST(MidiMessageToString, ProgramChangeFormat) {
    auto s = MidiMessage::programChange(2, 1).toString();
    EXPECT_NE(s.find("PC"), std::string::npos);
    EXPECT_NE(s.find("C1 01"), std::string::npos);
}
