// ControllerState loaded from the real data/midi_controller.json.
#include "gtest/gtest.h"
#include "mc/config/ConfigLoader.h"
#include "mc/domain/ControllerState.h"

using namespace mc;

TEST(ControllerState, GlobalSettings) {
    ControllerState s = config::loadControllerStateFromFile("data/midi_controller.json");
    EXPECT_EQ(s.mode, "standard");
    EXPECT_EQ(s.knobColor, "Yellow");
    EXPECT_EQ(s.knobBrightness, 100);
    EXPECT_FALSE(s.buttonsLocked);
    EXPECT_EQ(s.apiPort, 8090);
    EXPECT_EQ(s.tempo, 106);
    EXPECT_EQ(s.currentSet, "2017-05-22 Set");
    EXPECT_EQ(s.currentSong, "Victory - Rhythm");
    EXPECT_EQ(s.currentPart, "Chorus");
}

TEST(ControllerState, ChannelTable) {
    ControllerState s = config::loadControllerStateFromFile("data/midi_controller.json");
    ASSERT_EQ(s.channels.size(), 6u);  // null channels dropped
    // Ascending channel order.
    EXPECT_EQ(s.channels.front().channel, 1);
    EXPECT_EQ(s.channels.front().name, "QuartzV2");
    EXPECT_EQ(s.channels.back().channel, 16);
    EXPECT_EQ(s.channels.back().name, "ScarlettLove");

    const ChannelConfig* tl = s.channelForPedal("TimeLine");
    ASSERT_NE(tl, nullptr);
    EXPECT_EQ(tl->channel, 2);
    EXPECT_TRUE(tl->state);

    EXPECT_EQ(s.channelForPedal("QuartzV2")->preset, Value{2L});             // number
    EXPECT_EQ(s.channelForPedal("ScarlettLove")->preset, Value{std::string("Plexi")});  // name
}

TEST(ControllerState, ButtonSetup) {
    ControllerState s = config::loadControllerStateFromFile("data/midi_controller.json");
    ASSERT_EQ(s.buttons.size(), 5u);
    const ButtonConfig* b1 = s.button(1);
    ASSERT_NE(b1, nullptr);
    EXPECT_EQ(b1->function, "Part Dn");
    EXPECT_EQ(b1->longPressFunc, "Select Part Dn");

    const ButtonConfig* b2 = s.button(2);
    ASSERT_NE(b2, nullptr);
    EXPECT_EQ(b2->function, "Select");
    EXPECT_EQ(b2->longPressFunc, "");  // no long-press func for Select
}
