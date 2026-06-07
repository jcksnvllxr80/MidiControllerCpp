// Setlist/Song/Part loaded from the real converted fixtures.
#include "gtest/gtest.h"
#include "mc/config/ConfigLoader.h"
#include "mc/domain/SongPartSet.h"

using namespace mc;

TEST(SongPartSet, VictoryRhythmStructure) {
    Song s = config::loadSongFromFile("data/songs/Victory - Rhythm.json");
    EXPECT_EQ(s.name, "Victory - Rhythm");
    EXPECT_EQ(s.bpm, "110");
    EXPECT_DOUBLE_EQ(s.bpmValue(), 110.0);

    ASSERT_EQ(s.parts.size(), 2u);
    EXPECT_EQ(s.parts[0].name, "Chorus");  // position 1
    EXPECT_EQ(s.parts[1].name, "Bridge");  // position 2
    EXPECT_EQ(s.indexOfPart("Bridge"), 1);
}

TEST(SongPartSet, PedalStatesAndPresetTypes) {
    Song s = config::loadSongFromFile("data/songs/Victory - Rhythm.json");
    const Part* chorus = s.part("Chorus");
    ASSERT_NE(chorus, nullptr);

    const PedalState* tl = chorus->pedal("TimeLine");
    ASSERT_NE(tl, nullptr);
    EXPECT_FALSE(tl->engaged);
    EXPECT_EQ(tl->preset, Value{2L});  // int preset

    const PedalState* sl = chorus->pedal("ScarlettLove");
    ASSERT_NE(sl, nullptr);
    EXPECT_TRUE(sl->engaged);
    EXPECT_EQ(sl->preset, Value{std::string("Plexi")});  // name preset
}

TEST(SongPartSet, ParamOrderPreservedAndEngagedRef) {
    Song s = config::loadSongFromFile("data/songs/Victory - Rhythm.json");
    const PedalState* ss = s.part("Chorus")->pedal("SuperSwitcher");
    ASSERT_NE(ss, nullptr);
    ASSERT_EQ(ss->params.size(), 9u);
    // Order must match the YAML (drives MIDI emission order).
    EXPECT_EQ(ss->params[0].first, "Loop 1");
    EXPECT_EQ(ss->params[1].first, "Loop 2");
    EXPECT_EQ(ss->params[4].first, "Loop 6");
    EXPECT_EQ(ss->params[7].first, "Control 1");
    EXPECT_EQ(ss->params[8].first, "Control 2");
    const Value expectedLoop1 = EngagedRef{"Morning Glory", false};
    EXPECT_EQ(ss->params[0].second, expectedLoop1);
}

TEST(SongPartSet, NullParamsBecomeEmpty) {
    Song s = config::loadSongFromFile("data/songs/Victory - Rhythm.json");
    const PedalState* ir = s.part("Chorus")->pedal("Iridium");
    ASSERT_NE(ir, nullptr);
    EXPECT_TRUE(ir->params.empty());  // "params": null in the song
    EXPECT_EQ(ir->preset, Value{1L});
}

TEST(SongPartSet, SetlistLoadsAllSongsInOrder) {
    Setlist sl = config::loadSetlistFromFile("data/sets/2017-05-22 Set.json", "data/songs");
    EXPECT_EQ(sl.name, "2017-05-22 Set");
    ASSERT_EQ(sl.songs.size(), 4u);
    EXPECT_EQ(sl.songs[0].name, "Victory - Rhythm");
    EXPECT_EQ(sl.songs[1].name, "Here As In Heaven - Lead");
    EXPECT_EQ(sl.songs[2].name, "Victory - Lead");
    EXPECT_EQ(sl.songs[3].name, "Here As In Heaven - Rhythm");
    EXPECT_EQ(sl.indexOfSong("Victory - Lead"), 2);
}
