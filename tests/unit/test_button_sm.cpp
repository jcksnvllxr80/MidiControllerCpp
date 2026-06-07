#include "mc/domain/ButtonSM.h"

#include "gtest/gtest.h"
#include "support/FakeClock.h"

using namespace mc;
using namespace mc::test;
using PK = ButtonSM::PressKind;

TEST(ButtonSM, ShortPressYieldsPrimaryAction) {
    FakeClock clk;
    ButtonSM b("Part Dn", "Select Part Dn", clk);
    EXPECT_EQ(b.onEdge(false).kind, PK::None);  // press
    clk.advance(0.2);
    auto r = b.onEdge(true);  // release
    EXPECT_EQ(r.kind, PK::Short);
    EXPECT_EQ(r.action, "Part Dn");
    EXPECT_FALSE(b.isPressed());
}

TEST(ButtonSM, LongPressYieldsSecondaryAction) {
    FakeClock clk;
    ButtonSM b("Part Dn", "Select Part Dn", clk);
    b.onEdge(false);
    clk.advance(0.8);  // >= 0.5s
    auto r = b.onEdge(true);
    EXPECT_EQ(r.kind, PK::Long);
    EXPECT_EQ(r.action, "Select Part Dn");
}

TEST(ButtonSM, ExactlyThresholdIsLong) {
    FakeClock clk;
    ButtonSM b("X", "Y", clk);
    b.onEdge(false);
    clk.advance(0.5);  // not < 0.5
    EXPECT_EQ(b.onEdge(true).kind, PK::Long);
}

TEST(ButtonSM, PartnerDoublePressWithinWindow) {
    FakeClock clk;
    clk.set(10.0);
    ButtonSM a("A", "", clk);
    ButtonSM b("B", "", clk);
    a.setPartner(&b);
    // Partner just acted and flagged a config change, within the 0.25s window.
    b.setLastActionTime(10.0);
    b.setPedalConfigChanged(true);

    a.onEdge(false);   // press at t=10
    clk.set(10.1);     // release 0.1s later (<= 0.25 window)
    auto r = a.onEdge(true);
    EXPECT_EQ(r.kind, PK::Partner);
    EXPECT_EQ(r.action, "partner func");
    EXPECT_FALSE(b.pedalConfigChanged());  // cleared by the partner branch
}

TEST(ButtonSM, PartnerFlagStaleFallsBackToOwnBranch) {
    FakeClock clk;
    clk.set(10.0);
    ButtonSM a("A", "AL", clk);
    ButtonSM b("B", "", clk);
    a.setPartner(&b);
    b.setLastActionTime(10.0);
    b.setPedalConfigChanged(true);

    a.onEdge(false);
    clk.set(10.4);  // 0.4s > 0.25 window -> own branch
    auto r = a.onEdge(true);
    EXPECT_EQ(r.kind, PK::Short);  // 0.4 < 0.5
    EXPECT_EQ(r.action, "A");
}
