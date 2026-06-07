// Engage / Bypass / Toggle / Set Preset across all real pedals, plus state
// transitions and error paths.
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mc/domain/MidiPedal.h"
#include "support/Fixtures.h"
#include "support/RecordingMidiOut.h"

using namespace mc;
using namespace mc::test;
using Bytes = std::vector<uint8_t>;
using Seq = std::vector<std::vector<uint8_t>>;

namespace {
MidiPedal make(const std::string& name, RecordingMidiOut& out) {
    return MidiPedal(name, channelFor(name), &pedalConfig(name), &out);
}
}  // namespace

// ---- engage / bypass / toggle ----------------------------------------------

TEST(PedalAction, TimeLineEngageBypassToggle) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.turnOn();
    EXPECT_TRUE(p.isEngaged());
    p.turnOff();
    EXPECT_FALSE(p.isEngaged());
    p.toggle();  // no Toggle Bypass config -> nothing
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB1, 0x66, 0x7F}, {0xB1, 0x66, 0x00}}));
}

TEST(PedalAction, IridiumEngageBypass) {
    RecordingMidiOut out;
    auto p = make("Iridium", out);
    p.turnOn();
    p.turnOff();
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB2, 0x66, 0x7F}, {0xB2, 0x66, 0x00}}));
}

TEST(PedalAction, BigSkyEngageBypass) {
    RecordingMidiOut out;
    auto p = make("BigSky", out);
    p.turnOn();
    p.turnOff();
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB3, 0x66, 0x7F}, {0xB3, 0x66, 0x00}}));
}

TEST(PedalAction, ScarlettLoveEngageBypassToggleFlips) {
    RecordingMidiOut out;
    auto p = make("ScarlettLove", out);
    EXPECT_FALSE(p.isEngaged());
    p.turnOn();
    EXPECT_TRUE(p.isEngaged());
    p.turnOff();
    EXPECT_FALSE(p.isEngaged());
    p.toggle();  // cc29 val1, flips engaged false->true
    EXPECT_TRUE(p.isEngaged());
    p.toggle();
    EXPECT_FALSE(p.isEngaged());
    EXPECT_EQ(out.byteSeq(),
              (Seq{{0xBF, 0x17, 0x01}, {0xBF, 0x18, 0x01}, {0xBF, 0x1D, 0x01}, {0xBF, 0x1D, 0x01}}));
}

TEST(PedalAction, QuartzNoEngageBypassConfigIsSilent) {
    RecordingMidiOut out;
    auto p = make("QuartzV2", out);
    p.turnOn();
    p.turnOff();
    p.toggle();
    EXPECT_TRUE(out.messages.empty());
    EXPECT_FALSE(p.isEngaged());  // turnOn with no Engage config does not flip state
}

TEST(PedalAction, SuperSwitcherNoEngageBypassConfigIsSilent) {
    RecordingMidiOut out;
    auto p = make("SuperSwitcher", out);
    p.turnOn();
    p.turnOff();
    EXPECT_TRUE(out.messages.empty());
}

// ---- set preset across pedals ----------------------------------------------

struct PresetCase {
    std::string pedal;
    Value preset;
    bool emit;
    Bytes expected;
    std::string id;
};
class PedalPreset : public ::testing::TestWithParam<PresetCase> {};
TEST_P(PedalPreset, Emits) {
    const auto& c = GetParam();
    RecordingMidiOut out;
    auto p = make(c.pedal, out);
    p.setPreset(c.preset);
    if (c.emit) {
        ASSERT_EQ(out.messages.size(), 1u);
        EXPECT_EQ(out.messages[0].bytes(), c.expected);
    } else {
        EXPECT_TRUE(out.messages.empty());
    }
}
INSTANTIATE_TEST_SUITE_P(
    Cases, PedalPreset,
    ::testing::Values(
        PresetCase{"QuartzV2", Value{0L}, true, {0xB0, 0x61, 0x00}, "quartz0"},
        PresetCase{"QuartzV2", Value{64L}, true, {0xB0, 0x61, 0x40}, "quartz64"},
        PresetCase{"QuartzV2", Value{127L}, true, {0xB0, 0x61, 0x7F}, "quartz127"},
        PresetCase{"TimeLine", Value{0L}, true, {0xC1, 0x00}, "tl0"},
        PresetCase{"TimeLine", Value{2L}, true, {0xC1, 0x02}, "tl2"},
        PresetCase{"TimeLine", Value{130L}, true, {0xC1, 0x02}, "tl130_wraps"},
        PresetCase{"Iridium", Value{1L}, true, {0xC2, 0x01}, "ir1"},
        PresetCase{"BigSky", Value{1L}, true, {0xC3, 0x01}, "bs1"},
        PresetCase{"BigSky", Value{200L}, true, {0xC3, 0x48}, "bs200_wraps"},
        PresetCase{"SuperSwitcher", Value{1L}, true, {0xC4, 0x01}, "ss1"},
        PresetCase{"SuperSwitcher", Value{100L}, true, {0xC4, 0x64}, "ss100"},
        PresetCase{"ScarlettLove", Value{std::string("Plexi")}, true, {0xBF, 0x15, 0x7F}, "sl_plexi"},
        PresetCase{"ScarlettLove", Value{std::string("TS808")}, true, {0xBF, 0x14, 0x7F}, "sl_ts808"},
        PresetCase{"ScarlettLove", Value{std::string("Klone")}, true, {0xBF, 0x16, 0x7F}, "sl_klone"},
        // empty / none are no-ops on every pedal
        PresetCase{"TimeLine", Value{std::string("")}, false, {}, "tl_empty"},
        PresetCase{"TimeLine", Value{std::monostate{}}, false, {}, "tl_none"},
        PresetCase{"QuartzV2", Value{std::string("")}, false, {}, "quartz_empty"}),
    [](const ::testing::TestParamInfo<PresetCase>& i) { return i.param.id; });

TEST(PedalAction, SetPresetStoresValue) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);
    p.setPreset(Value{5L});
    EXPECT_EQ(p.preset(), Value{5L});
}

TEST(PedalAction, ScarlettLoveUnknownPresetThrows) {
    RecordingMidiOut out;
    auto p = make("ScarlettLove", out);
    // lookup miss -> None -> requireInt throws (Python chr(None) would crash too).
    EXPECT_THROW(p.setPreset(Value{std::string("Nonexistent")}), std::runtime_error);
}

TEST(PedalAction, SetTempoMultiValueMutationQuirk) {
    RecordingMidiOut out;
    auto p = make("QuartzV2", out);
    p.setTempo(Value{110L});
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB0, 0x5E, 0x01}, {0xB0, 0x5E, 0x01}}));
    out.clear();
    p.setTempo(Value{250L});  // 250/100=2 ; then 2%100=2
    EXPECT_EQ(out.byteSeq(), (Seq{{0xB0, 0x5E, 0x02}, {0xB0, 0x5E, 0x02}}));
}

TEST(PedalAction, SetTempoEmptyAndNoneAreNoOps) {
    RecordingMidiOut out;
    auto p = make("QuartzV2", out);
    p.setTempo(Value{std::string("")});
    p.setTempo(Value{std::monostate{}});
    EXPECT_TRUE(out.messages.empty());
}

TEST(PedalAction, SetTempoNoConfigIsSilent) {
    RecordingMidiOut out;
    auto p = make("TimeLine", out);  // TimeLine has no Set Tempo
    p.setTempo(Value{120L});
    EXPECT_TRUE(out.messages.empty());
}
