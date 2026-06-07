// Additional ButtonSM coverage: edges, boundaries, partner wiring, accessors.
#include "gtest/gtest.h"
#include "mc/domain/ButtonSM.h"
#include "support/FakeClock.h"

using namespace mc;
using namespace mc::test;
using PK = ButtonSM::PressKind;

TEST(ButtonSMMore, PressReturnsNoneAndSetsPressed) {
    FakeClock clk;
    ButtonSM b("A", "AL", clk);
    auto r = b.onEdge(false);
    EXPECT_EQ(r.kind, PK::None);
    EXPECT_TRUE(b.isPressed());
}

TEST(ButtonSMMore, ReleaseClearsPressed) {
    FakeClock clk;
    ButtonSM b("A", "AL", clk);
    b.onEdge(false);
    clk.advance(0.1);
    b.onEdge(true);
    EXPECT_FALSE(b.isPressed());
}

struct DurCase {
    double dt;
    PK kind;
    const char* action;
    std::string id;
};
class ButtonDur : public ::testing::TestWithParam<DurCase> {};
TEST_P(ButtonDur, Classifies) {
    const auto& c = GetParam();
    FakeClock clk;
    ButtonSM b("PRIMARY", "SECONDARY", clk);
    b.onEdge(false);
    clk.advance(c.dt);
    auto r = b.onEdge(true);
    EXPECT_EQ(r.kind, c.kind);
    EXPECT_EQ(r.action, c.action);
}
INSTANTIATE_TEST_SUITE_P(Durations, ButtonDur,
                         ::testing::Values(DurCase{0.0, PK::Short, "PRIMARY", "zero"},
                                           DurCase{0.25, PK::Short, "PRIMARY", "quarter"},
                                           DurCase{0.499, PK::Short, "PRIMARY", "just_under"},
                                           DurCase{0.5, PK::Long, "SECONDARY", "at_threshold"},
                                           DurCase{0.75, PK::Long, "SECONDARY", "long"},
                                           DurCase{3.0, PK::Long, "SECONDARY", "very_long"}),
                         [](const ::testing::TestParamInfo<DurCase>& i) { return i.param.id; });

TEST(ButtonSMMore, MultipleCycles) {
    FakeClock clk;
    ButtonSM b("A", "AL", clk);
    for (int i = 0; i < 5; ++i) {
        b.onEdge(false);
        clk.advance(0.1);
        EXPECT_EQ(b.onEdge(true).kind, PK::Short);
        clk.advance(1.0);
    }
}

TEST(ButtonSMMore, PartnerIsBidirectional) {
    FakeClock clk;
    ButtonSM a("A", "", clk);
    ButtonSM b("B", "", clk);
    a.setPartner(&b);
    // Both directions wired: trigger partner branch from b's side too.
    a.setLastActionTime(clk.now());
    a.setPedalConfigChanged(true);
    b.onEdge(false);
    auto r = b.onEdge(true);  // within window of a's recent action
    EXPECT_EQ(r.kind, PK::Partner);
}

TEST(ButtonSMMore, NoPartnerUsesOwnBranch) {
    FakeClock clk;
    ButtonSM b("A", "AL", clk);
    b.onEdge(false);
    clk.advance(0.2);
    EXPECT_EQ(b.onEdge(true).kind, PK::Short);
}

TEST(ButtonSMMore, Accessors) {
    FakeClock clk;
    clk.set(5.0);
    ButtonSM b("Primary", "Secondary", clk);
    EXPECT_EQ(b.name(), "Primary");
    EXPECT_EQ(b.longPressFunc(), "Secondary");
    b.setLastActionTime(12.5);
    EXPECT_DOUBLE_EQ(b.lastActionTime(), 12.5);
    b.setPedalConfigChanged(true);
    EXPECT_TRUE(b.pedalConfigChanged());
    b.setPedalConfigChanged(false);
    EXPECT_FALSE(b.pedalConfigChanged());
}

TEST(ButtonSMMore, SetPartnerNullIsSafe) {
    FakeClock clk;
    ButtonSM b("A", "AL", clk);
    b.setPartner(nullptr);  // no-op, must not crash
    b.onEdge(false);
    clk.advance(0.1);
    EXPECT_EQ(b.onEdge(true).kind, PK::Short);
}
