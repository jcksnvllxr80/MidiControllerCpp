// Loader branches not covered elsewhere: multi as array, file wrappers, float
// tempo, and default-filling when sections are absent.
#include "gtest/gtest.h"
#include "mc/config/ConfigLoader.h"

using namespace mc;
using namespace mc::config;

TEST(LoaderMore, MultiAsArray) {
    auto c = loadPedalConfigFromString(R"({"Set Preset":{"multi":["a","b"],"a":{"cc":5},"b":{"cc":6}}})");
    const Action* sp = c.setPreset();
    ASSERT_NE(sp, nullptr);
    ASSERT_EQ(sp->multi.size(), 2u);
    EXPECT_EQ(sp->multi[0], "a");
    EXPECT_EQ(sp->multi[1], "b");
}

TEST(LoaderMore, PedalConfigFromFile) {
    auto c = loadPedalConfigFromFile("data/pedals/TimeLine.json");
    ASSERT_NE(c.engage(), nullptr);
    EXPECT_EQ(c.engage()->cc.value_or(-1), 102);
}

TEST(LoaderMore, ControllerStateFromFile) {
    auto st = loadControllerStateFromFile("data/midi_controller.json");
    EXPECT_EQ(st.currentSet, "2017-05-22 Set");
    EXPECT_EQ(st.channels.size(), 6u);
}

TEST(LoaderMore, SongFromFile) {
    auto s = loadSongFromFile("data/songs/Victory - Rhythm.json");
    EXPECT_EQ(s.name, "Victory - Rhythm");
    EXPECT_EQ(s.parts.size(), 2u);
}

TEST(LoaderMore, SetlistFromFile) {
    auto sl = loadSetlistFromFile("data/sets/2016-09-16 Set.json", "data/songs");
    EXPECT_EQ(sl.name, "2016-09-16 Set");
    EXPECT_EQ(sl.songs.size(), 2u);
}

TEST(LoaderMore, TempoFloatParses) {
    auto s = loadSongFromString(R"({"name":"S","tempo":96.5,"parts":{}})");
    EXPECT_DOUBLE_EQ(s.bpmValue(), 96.5);
}

TEST(LoaderMore, EmptyControllerJsonUsesDefaults) {
    auto st = loadControllerStateFromString("{}");
    EXPECT_EQ(st.mode, "standard");
    EXPECT_EQ(st.apiPort, 8090);
    EXPECT_FALSE(st.buttonsLocked);
    EXPECT_TRUE(st.channels.empty());
    EXPECT_TRUE(st.buttons.empty());
    EXPECT_EQ(st.tempo, 0);
}

TEST(LoaderMore, SongWithoutPartsIsEmpty) {
    auto s = loadSongFromString(R"({"name":"S","tempo":100})");
    EXPECT_TRUE(s.parts.empty());
}

TEST(LoaderMore, EmptyPedalConfigHasNoGroups) {
    auto c = loadPedalConfigFromString("{}");
    EXPECT_EQ(c.engage(), nullptr);
    EXPECT_EQ(c.setPreset(), nullptr);
    EXPECT_TRUE(c.root.children.empty());
}
