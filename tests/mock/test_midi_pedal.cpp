// MidiPedal against the *real* converted pedal configs (data/pedals/*.json).
// These lock the exact MIDI byte sequences the Python brain produced, including
// a few load-bearing quirks (cc:0 bank skipped, multi val-mutation in tempo).
#include <map>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mc/config/ConfigLoader.h"
#include "mc/domain/MidiPedal.h"
#include "support/MockMidiOut.h"
#include "support/RecordingMidiOut.h"

using namespace mc;
using namespace mc::test;
using Bytes = std::vector<uint8_t>;
using Seq = std::vector<std::vector<uint8_t>>;
using ::testing::InSequence;

namespace {

// Channels per midi_controller.yaml.
constexpr int kQuartz = 1, kTimeLine = 2, kIridium = 3, kBigSky = 4, kSuper = 5, kScarlett = 16;

class MidiPedalTest : public ::testing::Test {
protected:
    const PedalConfig& config(const std::string& name) {
        auto it = cfgs_.find(name);
        if (it == cfgs_.end())
            it = cfgs_.emplace(name, config::loadPedalConfigFromFile("data/pedals/" + name + ".json")).first;
        return it->second;
    }
    RecordingMidiOut out_;

private:
    std::map<std::string, PedalConfig> cfgs_;
};

}  // namespace

TEST_F(MidiPedalTest, TimeLineEngageBypass) {
    MidiPedal p("TimeLine", kTimeLine, &config("TimeLine"), &out_);
    p.turnOn();
    p.turnOff();
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xB1, 0x66, 0x7F}, {0xB1, 0x66, 0x00}}));
    EXPECT_FALSE(p.isEngaged());
}

TEST_F(MidiPedalTest, TimeLineSetPresetSendsOnlyProgramChange) {
    // The 'bank' sub-action has cc:0 (falsy in Python) so it is skipped; only the
    // 'preset' program change (x % 128) is emitted.
    MidiPedal p("TimeLine", kTimeLine, &config("TimeLine"), &out_);
    p.setPreset(Value{2L});
    p.setPreset(Value{1L});
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xC1, 0x02}, {0xC1, 0x01}}));
}

TEST_F(MidiPedalTest, TimeLinePresetAbove127WrapsViaModulo) {
    MidiPedal p("TimeLine", kTimeLine, &config("TimeLine"), &out_);
    p.setPreset(Value{130L});  // 130 % 128 = 2  (bank still not sent)
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xC1, 0x02}}));
}

TEST_F(MidiPedalTest, ScarlettLovePresetIsDictLookupControlChange) {
    // control change func maps the preset *name* to a CC number; value byte = 127.
    MidiPedal p("ScarlettLove", kScarlett, &config("ScarlettLove"), &out_);
    p.setPreset(Value{std::string("Plexi")});
    p.setPreset(Value{std::string("TS808")});
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xBF, 0x15, 0x7F}, {0xBF, 0x14, 0x7F}}));
}

TEST_F(MidiPedalTest, ScarlettLoveEngageBypassToggle) {
    MidiPedal p("ScarlettLove", kScarlett, &config("ScarlettLove"), &out_);
    p.turnOn();   // Engage cc:23 val:1
    p.turnOff();  // Bypass cc:24 val:1
    p.toggle();   // Toggle Bypass cc:29 val:1
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xBF, 0x17, 0x01}, {0xBF, 0x18, 0x01}, {0xBF, 0x1D, 0x01}}));
}

TEST_F(MidiPedalTest, QuartzHasNoEngageSoTurnOnIsSilent) {
    MidiPedal p("QuartzV2", kQuartz, &config("QuartzV2"), &out_);
    p.turnOn();   // no 'Engage' in config -> nothing emitted
    p.turnOff();  // no 'Bypass'
    EXPECT_TRUE(out_.messages.empty());
}

TEST_F(MidiPedalTest, QuartzSetPresetIsControlChange) {
    MidiPedal p("QuartzV2", kQuartz, &config("QuartzV2"), &out_);
    p.setPreset(Value{0L});
    p.setPreset(Value{1L});
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xB0, 0x61, 0x00}, {0xB0, 0x61, 0x01}}));
}

TEST_F(MidiPedalTest, QuartzSetTempoMultiMutatesValueFaithfully) {
    // Faithful quirk: 'tens_ones' (x % 100) operates on the already-divided value,
    // so tempo 110 emits CC#94=1 then CC#94=1 (not 1 then 10).
    MidiPedal p("QuartzV2", kQuartz, &config("QuartzV2"), &out_);
    p.setTempo(Value{110L});
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xB0, 0x5E, 0x01}, {0xB0, 0x5E, 0x01}}));
}

TEST_F(MidiPedalTest, SuperSwitcherPresetIsProgramChange) {
    MidiPedal p("SuperSwitcher", kSuper, &config("SuperSwitcher"), &out_);
    p.setPreset(Value{1L});
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xC4, 0x01}}));
}

TEST_F(MidiPedalTest, SuperSwitcherEngagedDictParamsMapToOnOff) {
    MidiPedal p("SuperSwitcher", kSuper, &config("SuperSwitcher"), &out_);
    MidiPedal::Params params = {
        {"Loop 1", EngagedRef{"Morning Glory", true}},   // cc102 -> on 127
        {"Loop 2", EngagedRef{"Angry Charlie", false}},  // cc103 -> off 0
        {"Control 1", EngagedRef{"Superbolt Heavy", false}},  // cc112 -> off 0
    };
    p.setParams(params);
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xB4, 0x66, 0x7F}, {0xB4, 0x67, 0x00}, {0xB4, 0x70, 0x00}}));
}

TEST_F(MidiPedalTest, BigSkyAndIridiumPresetMultiProgramChange) {
    MidiPedal big("BigSky", kBigSky, &config("BigSky"), &out_);
    big.setPreset(Value{1L});
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xC3, 0x01}}));

    out_.clear();
    MidiPedal ir("Iridium", kIridium, &config("Iridium"), &out_);
    ir.turnOn();              // Engage cc102 val127
    ir.setPreset(Value{1L});  // PC 1
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xB2, 0x66, 0x7F}, {0xC2, 0x01}}));
}

TEST_F(MidiPedalTest, TimeLineKnobDictParam) {
    // 'Type' knob: dict maps Digital->2 on cc19.
    MidiPedal p("TimeLine", kTimeLine, &config("TimeLine"), &out_);
    p.setParams({{"Type", Value{std::string("Digital")}}});
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xB1, 0x13, 0x02}}));
}

TEST_F(MidiPedalTest, TimeLineMinMaxParamInRangeAndOut) {
    MidiPedal p("TimeLine", kTimeLine, &config("TimeLine"), &out_);
    p.setParams({{"Time", Value{64L}}});  // cc3, in [0,127]
    EXPECT_EQ(out_.byteSeq(), (Seq{{0xB1, 0x03, 0x40}}));
    out_.clear();
    p.setParams({{"Time", Value{200L}}});  // out of range -> not sent
    EXPECT_TRUE(out_.messages.empty());
}

// --- GoogleMock expectation-style (ordered) ----------------------------------
TEST_F(MidiPedalTest, GmockOrderedEngageThenPreset) {
    test::MockMidiOut mock;
    MidiPedal p("TimeLine", kTimeLine, &config("TimeLine"), &mock);
    InSequence seq;
    EXPECT_CALL(mock, send(MidiMessage::controlChange(2, 102, 127)));
    EXPECT_CALL(mock, send(MidiMessage::programChange(2, 2)));
    p.turnOn();
    p.setPreset(Value{2L});
}
