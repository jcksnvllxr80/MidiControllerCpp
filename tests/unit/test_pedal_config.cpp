// Action / PedalConfig helpers, plus structural assertions on the real configs.
#include "gtest/gtest.h"
#include "mc/domain/PedalConfig.h"
#include "support/Fixtures.h"

using namespace mc;
using namespace mc::test;

// ---- helpers on hand-built nodes -------------------------------------------

TEST(Action, ChildLookup) {
    Action a;
    a.children.push_back({"alpha", Action{}});
    a.children.push_back({"beta", Action{}});
    EXPECT_NE(a.child("alpha"), nullptr);
    EXPECT_NE(a.child("beta"), nullptr);
    EXPECT_EQ(a.child("missing"), nullptr);
}

TEST(Action, DictGetIncludingZero) {
    Action a;
    a.dict = {{"k", 5}, {"zero", 0}};
    EXPECT_EQ(a.dictGet("k").value_or(-1), 5);
    EXPECT_EQ(a.dictGet("zero").value_or(-1), 0);  // 0 is a real value, not "missing"
    EXPECT_FALSE(a.dictGet("nope").has_value());
}

TEST(Action, HasTruthyCc) {
    Action a;
    EXPECT_FALSE(a.hasTruthyCc());  // absent
    a.cc = 0;
    EXPECT_FALSE(a.hasTruthyCc());  // zero is falsy (Python parity)
    a.cc = 102;
    EXPECT_TRUE(a.hasTruthyCc());
}

TEST(PedalConfig, GroupAccessors) {
    PedalConfig c;
    c.root.children.push_back({"Engage", Action{}});
    c.root.children.push_back({"Knobs/Switches", Action{}});
    EXPECT_NE(c.engage(), nullptr);
    EXPECT_NE(c.paramGroup("Knobs/Switches"), nullptr);
    EXPECT_EQ(c.bypass(), nullptr);
    EXPECT_EQ(c.setPreset(), nullptr);
    EXPECT_EQ(c.group("Nope"), nullptr);
}

// ---- real config structure --------------------------------------------------

TEST(RealConfig, TimeLineEngageBypass) {
    const PedalConfig& c = pedalConfig("TimeLine");
    ASSERT_NE(c.engage(), nullptr);
    EXPECT_EQ(c.engage()->cc.value_or(-1), 102);
    EXPECT_EQ(c.engage()->value.value_or(-1), 127);
    ASSERT_NE(c.bypass(), nullptr);
    EXPECT_EQ(c.bypass()->value.value_or(-1), 0);
    EXPECT_EQ(c.toggleBypass(), nullptr);
}

TEST(RealConfig, TimeLineSetPresetMulti) {
    const PedalConfig& c = pedalConfig("TimeLine");
    const Action* sp = c.setPreset();
    ASSERT_NE(sp, nullptr);
    ASSERT_EQ(sp->multi.size(), 2u);
    EXPECT_EQ(sp->multi[0], "bank");
    EXPECT_EQ(sp->multi[1], "preset");

    const Action* bank = sp->child("bank");
    ASSERT_NE(bank, nullptr);
    EXPECT_EQ(bank->cc.value_or(-1), 0);          // the falsy-cc quirk source
    EXPECT_FALSE(bank->hasTruthyCc());
    EXPECT_EQ(bank->func.kind(), Transform::Kind::Arithmetic);
    EXPECT_EQ(bank->func.op(), '/');
    EXPECT_EQ(bank->func.operand(), 128);

    const Action* preset = sp->child("preset");
    ASSERT_NE(preset, nullptr);
    ASSERT_TRUE(preset->programChange.has_value());
    EXPECT_EQ(preset->programChange->transform.op(), '%');
    EXPECT_EQ(preset->programChange->transform.operand(), 128);
}

TEST(RealConfig, TimeLineKnobDicts) {
    const PedalConfig& c = pedalConfig("TimeLine");
    const Action* ks = c.paramGroup("Knobs/Switches");
    ASSERT_NE(ks, nullptr);
    const Action* type = ks->child("Type");
    ASSERT_NE(type, nullptr);
    EXPECT_EQ(type->cc.value_or(-1), 19);
    EXPECT_EQ(type->dict.size(), 12u);
    EXPECT_EQ(type->dictGet("Digital").value_or(-1), 2);
    EXPECT_EQ(type->dictGet("Lo-Fi").value_or(-1), 11);

    const Action* time = ks->child("Time");
    ASSERT_NE(time, nullptr);
    EXPECT_EQ(time->cc.value_or(-1), 3);
    EXPECT_EQ(time->min.value_or(-1), 0);
    EXPECT_EQ(time->max.value_or(-1), 127);

    const Action* fa = ks->child("Footswitch A");
    ASSERT_NE(fa, nullptr);
    EXPECT_EQ(fa->on.value_or(-1), 0);
    EXPECT_EQ(fa->off.value_or(-1), 127);
}

TEST(RealConfig, ScarlettLoveControlChangeLookup) {
    const PedalConfig& c = pedalConfig("ScarlettLove");
    const Action* sp = c.setPreset();
    ASSERT_NE(sp, nullptr);
    ASSERT_TRUE(sp->controlChange.has_value());
    EXPECT_EQ(sp->controlChange->transform.kind(), Transform::Kind::Lookup);
    EXPECT_EQ(sp->controlChange->transform.table().size(), 3u);
    EXPECT_EQ(sp->controlChange->options.size(), 3u);
    EXPECT_EQ(c.engage()->cc.value_or(-1), 23);
    EXPECT_EQ(c.bypass()->cc.value_or(-1), 24);
    ASSERT_NE(c.toggleBypass(), nullptr);
    EXPECT_EQ(c.toggleBypass()->cc.value_or(-1), 29);
}

TEST(RealConfig, QuartzNoEngageTempoMulti) {
    const PedalConfig& c = pedalConfig("QuartzV2");
    EXPECT_EQ(c.engage(), nullptr);
    EXPECT_EQ(c.bypass(), nullptr);
    ASSERT_NE(c.setPreset(), nullptr);
    EXPECT_EQ(c.setPreset()->cc.value_or(-1), 97);

    const Action* st = c.setTempo();
    ASSERT_NE(st, nullptr);
    ASSERT_EQ(st->multi.size(), 2u);
    EXPECT_EQ(st->child("hundreds")->cc.value_or(-1), 94);
    EXPECT_EQ(st->child("hundreds")->func.operand(), 100);
    EXPECT_EQ(st->child("hundreds")->func.op(), '/');
    EXPECT_EQ(st->child("tens_ones")->func.op(), '%');
}

TEST(RealConfig, SuperSwitcherProgramChangePreset) {
    const PedalConfig& c = pedalConfig("SuperSwitcher");
    const Action* sp = c.setPreset();
    ASSERT_NE(sp, nullptr);
    ASSERT_TRUE(sp->programChange.has_value());
    EXPECT_EQ(sp->programChange->transform.kind(), Transform::Kind::Identity);  // no func
    EXPECT_EQ(sp->programChange->min.value_or(-1), 0);
    EXPECT_EQ(sp->programChange->max.value_or(-1), 127);
    EXPECT_EQ(c.engage(), nullptr);
}

TEST(RealConfig, IridiumBankSelectHasFalsyCc) {
    const PedalConfig& c = pedalConfig("Iridium");
    const Action* params = c.paramGroup("Parameters");
    ASSERT_NE(params, nullptr);
    const Action* bank = params->child("Bank Select");
    ASSERT_NE(bank, nullptr);
    EXPECT_EQ(bank->cc.value_or(-1), 0);
    EXPECT_FALSE(bank->hasTruthyCc());  // so it never emits via setParams
}

TEST(RealConfig, EveryPedalParsesAndHasChannelGroups) {
    for (const auto& pedal : allPedals()) {
        const PedalConfig& c = pedalConfig(pedal);
        SCOPED_TRACE(pedal);
        EXPECT_FALSE(c.root.children.empty());  // parsed something
        // Every pedal in this rig has a Set Preset group.
        EXPECT_NE(c.setPreset(), nullptr);
    }
}
