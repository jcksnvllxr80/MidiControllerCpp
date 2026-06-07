// setParams details: multi-param ordering, group resolution & collisions,
// not-found, and EngagedRef handling.
#include <vector>

#include "gtest/gtest.h"
#include "mc/domain/MidiPedal.h"
#include "support/Fixtures.h"
#include "support/RecordingMidiOut.h"

using namespace mc;
using namespace mc::test;
using Seq = std::vector<std::vector<uint8_t>>;

namespace {
MidiPedal make(const std::string& name, RecordingMidiOut& out) {
    return MidiPedal(name, channelFor(name), &pedalConfig(name), &out);
}
}  // namespace

TEST(PedalParamsMore, MultipleParamsEmitInGivenOrder) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setParams({{"Type", Value{std::string("Digital")}},  // cc19 dict Digital=2
                 {"Time", Value{64L}},                       // cc3 min/max
                 {"Mix", Value{10L}}});                      // cc14 min/max
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB1, 0x13, 0x02}, {0xB1, 0x03, 0x40}, {0xB1, 0x0E, 0x0A}}));
}

TEST(PedalParamsMore, NameInKnobsAndParametersResolvesToKnobsFirst) {
    // TimeLine has "Filter" in Knobs/Switches (cc15) and a "Filter" sub-group in
    // Parameters (no cc). setParams must pick the Knobs/Switches one.
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setParams({{"Filter", Value{50L}}});
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB1, 0x0F, 0x32}}));  // cc15 = 0x0F, 50 = 0x32
}

TEST(PedalParamsMore, ParameterOnlyNameResolvesInParameters) {
    // "Boost" lives only in TimeLine Parameters (dict). First dict key " " -> 0.
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setParams({{"Boost", Value{std::string(" ")}}});
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB1, 0x17, 0x00}}));  // cc23 = 0x17
}

TEST(PedalParamsMore, UnknownParamEmitsNothing) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setParams({{"Nonexistent Param", Value{1L}}});
    EXPECT_TRUE(out.messages.empty());
}

TEST(PedalParamsMore, EmptyParamsEmitNothing) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setParams({});
    EXPECT_TRUE(out.messages.empty());
}

TEST(PedalParamsMore, SuperSwitcherEngagedRefSequenceInOrder) {
    RecordingMidiOut out;
    auto p = make("SuperSwitcher", out);
    p.setParams({
        {"Loop 1", EngagedRef{"Morning Glory", true}},   // cc102 on=127
        {"Loop 2", EngagedRef{"Angry Charlie", false}},  // cc103 off=0
        {"Loop 3", EngagedRef{"Superbolt", true}},       // cc104 on=127
        {"Control 1", EngagedRef{"X", false}},           // cc112 off=0
    });
    EXPECT_EQ(out.byteSeq(),
              (Seq{{0xB4, 0x66, 0x7F}, {0xB4, 0x67, 0x00}, {0xB4, 0x68, 0x7F}, {0xB4, 0x70, 0x00}}));
}

TEST(PedalParamsMore, OutOfRangeMinMaxParamSkipsButOthersStillEmit) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setParams({{"Time", Value{999L}},   // out of range -> skipped
                 {"Mix", Value{5L}}});    // emits
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB1, 0x0E, 0x05}}));
}

TEST(PedalParamsMore, SetSettingUnknownIsNoOp) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setSetting("Totally Unknown Setting");  // group not found -> nothing
    EXPECT_TRUE(out.messages.empty());
}

TEST(PedalParamsMore, IridiumBankSelectCc0NeverEmits) {
    // Bank Select has cc:0 (falsy) -> setParams must not emit.
    RecordingMidiOut out;
    auto p = make("Iridium", out);
    p.setParams({{"Bank Select", Value{std::string("bank 2")}}});
    EXPECT_TRUE(out.messages.empty());
}

TEST(PedalParamsMore, ValueOnlyParamRejectsNonMatchingInt) {
    // QuartzV2 "Tap Tempo" is {cc:93, value:1}: only the configured value (or
    // None, which falls back to it) resolves; any other int -> nothing sent.
    RecordingMidiOut out;
    auto p = make("QuartzV2", out);
    p.setParams({{"Tap Tempo", Value{1L}}});  // matches value
    p.setParams({{"Tap Tempo", Value{std::monostate{}}}});  // None -> value 1
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB0, 0x5D, 0x01}, {0xB0, 0x5D, 0x01}}));
    out.clear();
    p.setParams({{"Tap Tempo", Value{2L}}});  // no min/max, value mismatch -> drop
    EXPECT_TRUE(out.messages.empty());
}
