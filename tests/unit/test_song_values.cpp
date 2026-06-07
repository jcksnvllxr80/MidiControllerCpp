// Exact (song, part, pedal) value spot-checks across the real fixtures.
#include <map>
#include <string>

#include "gtest/gtest.h"
#include "mc/config/ConfigLoader.h"
#include "mc/domain/SongPartSet.h"

using namespace mc;

namespace {
const Song& song(const std::string& name) {
    static std::map<std::string, Song> cache;
    auto it = cache.find(name);
    if (it == cache.end())
        it = cache.emplace(name, config::loadSongFromFile("data/songs/" + name + ".json")).first;
    return it->second;
}
const PedalState& ped(const std::string& s, const std::string& part, const std::string& pedal) {
    return *song(s).part(part)->pedal(pedal);
}
}  // namespace

TEST(SongValues, HaihLeadIntro) {
    EXPECT_TRUE(ped("Here As In Heaven - Lead", "Intro", "ScarlettLove").engaged);
    EXPECT_EQ(ped("Here As In Heaven - Lead", "Intro", "ScarlettLove").preset, Value{std::string("TS808")});
    EXPECT_FALSE(ped("Here As In Heaven - Lead", "Intro", "TimeLine").engaged);
    EXPECT_EQ(ped("Here As In Heaven - Lead", "Intro", "TimeLine").preset, Value{0L});
    EXPECT_FALSE(ped("Here As In Heaven - Lead", "Intro", "BigSky").engaged);
    EXPECT_EQ(ped("Here As In Heaven - Lead", "Intro", "BigSky").preset, Value{0L});
    EXPECT_FALSE(ped("Here As In Heaven - Lead", "Intro", "QuartzV2").engaged);
    EXPECT_EQ(ped("Here As In Heaven - Lead", "Intro", "QuartzV2").preset, Value{1L});
    EXPECT_TRUE(ped("Here As In Heaven - Lead", "Intro", "Iridium").engaged);
    EXPECT_EQ(ped("Here As In Heaven - Lead", "Intro", "Iridium").preset, Value{1L});
    EXPECT_EQ(ped("Here As In Heaven - Lead", "Intro", "SuperSwitcher").preset, Value{1L});
}

TEST(SongValues, HaihLeadBridgeTimeLineEngaged) {
    EXPECT_TRUE(ped("Here As In Heaven - Lead", "Bridge", "TimeLine").engaged);
    EXPECT_EQ(ped("Here As In Heaven - Lead", "Bridge", "TimeLine").preset, Value{2L});
}

TEST(SongValues, VictoryRhythmChorusPresets) {
    EXPECT_TRUE(ped("Victory - Rhythm", "Chorus", "ScarlettLove").engaged);
    EXPECT_EQ(ped("Victory - Rhythm", "Chorus", "ScarlettLove").preset, Value{std::string("Plexi")});
    EXPECT_FALSE(ped("Victory - Rhythm", "Chorus", "TimeLine").engaged);
    EXPECT_EQ(ped("Victory - Rhythm", "Chorus", "TimeLine").preset, Value{2L});
    EXPECT_EQ(ped("Victory - Rhythm", "Chorus", "QuartzV2").preset, Value{0L});
}

TEST(SongValues, SuperSwitcherLoopEngagedDiffersByVersion) {
    const Value rhythmLoop1 = EngagedRef{"Morning Glory", false};
    const Value leadLoop1 = EngagedRef{"Morning Glory", true};
    EXPECT_EQ(ped("Victory - Rhythm", "Chorus", "SuperSwitcher").params[0].second, rhythmLoop1);
    EXPECT_EQ(ped("Victory - Lead", "Chorus", "SuperSwitcher").params[0].second, leadLoop1);
    EXPECT_EQ(ped("Here As In Heaven - Rhythm", "Intro", "SuperSwitcher").params[0].second, leadLoop1);
}

TEST(SongValues, SuperSwitcherParamNamesAndOrder) {
    const auto& params = ped("Victory - Rhythm", "Chorus", "SuperSwitcher").params;
    ASSERT_EQ(params.size(), 9u);
    EXPECT_EQ(params[0].first, "Loop 1");
    EXPECT_EQ(params[1].first, "Loop 2");
    EXPECT_EQ(params[2].first, "Loop 3");
    EXPECT_EQ(params[3].first, "Loop 4");
    EXPECT_EQ(params[4].first, "Loop 6");
    EXPECT_EQ(params[5].first, "Loop 7");
    EXPECT_EQ(params[6].first, "Loop 8");
    EXPECT_EQ(params[7].first, "Control 1");
    EXPECT_EQ(params[8].first, "Control 2");
}

TEST(SongValues, IridiumParamsNullAcrossVictory) {
    EXPECT_TRUE(ped("Victory - Rhythm", "Chorus", "Iridium").params.empty());
    EXPECT_TRUE(ped("Victory - Rhythm", "Bridge", "Iridium").params.empty());
}
