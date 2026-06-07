// Parametrized over every (song, part, pedal) in the real fixtures: each pedal
// must be present in each part with a well-typed preset. Plus per-song shape.
#include <map>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mc/config/ConfigLoader.h"
#include "mc/domain/SongPartSet.h"
#include "support/Fixtures.h"

using namespace mc;
using namespace mc::test;

namespace {

const Song& song(const std::string& name) {
    static std::map<std::string, Song> cache;
    auto it = cache.find(name);
    if (it == cache.end())
        it = cache.emplace(name, config::loadSongFromFile("data/songs/" + name + ".json")).first;
    return it->second;
}

struct PartPedal {
    std::string song;
    int partIdx;
    std::string pedal;
    std::string id;
};

std::vector<PartPedal> gen() {
    std::vector<PartPedal> out;
    for (const std::string& s : allSongs()) {
        const Song& sg = song(s);
        for (size_t i = 0; i < sg.parts.size(); ++i)
            for (const std::string& pedal : allPedals())
                out.push_back({s, static_cast<int>(i), pedal,
                               sanitize(s) + "_" + sanitize(sg.parts[i].name) + "_" + pedal});
    }
    return out;
}

}  // namespace

class PartPedalState : public ::testing::TestWithParam<PartPedal> {};

TEST_P(PartPedalState, PresentAndWellTyped) {
    const auto& c = GetParam();
    const Song& sg = song(c.song);
    ASSERT_LT(c.partIdx, static_cast<int>(sg.parts.size()));
    const Part& part = sg.parts[c.partIdx];
    const PedalState* st = part.pedal(c.pedal);
    ASSERT_NE(st, nullptr) << c.pedal << " missing from " << c.song << "/" << part.name;
    // Every pedal in these songs has a preset (int or non-empty name).
    const bool presetOk = isInt(st->preset) || (isString(st->preset) && !asString(st->preset)->empty());
    EXPECT_TRUE(presetOk) << "preset=" << toString(st->preset);
}

INSTANTIATE_TEST_SUITE_P(EverySongPart, PartPedalState, ::testing::ValuesIn(gen()),
                         [](const ::testing::TestParamInfo<PartPedal>& i) { return i.param.id; });

struct SongShape {
    std::string song;
    size_t parts;
    std::string bpm;
    std::string firstPart;
};
class SongShapeTest : public ::testing::TestWithParam<SongShape> {};
TEST_P(SongShapeTest, Matches) {
    const auto& c = GetParam();
    const Song& s = song(c.song);
    EXPECT_EQ(s.name, c.song);
    EXPECT_EQ(s.parts.size(), c.parts);
    EXPECT_EQ(s.bpm, c.bpm);
    ASSERT_FALSE(s.parts.empty());
    EXPECT_EQ(s.parts.front().name, c.firstPart);
}
INSTANTIATE_TEST_SUITE_P(
    AllSongs, SongShapeTest,
    ::testing::Values(SongShape{"Victory - Rhythm", 2, "110", "Chorus"},
                      SongShape{"Victory - Lead", 2, "110", "Chorus"},
                      SongShape{"Here As In Heaven - Lead", 4, "74", "Intro"},
                      SongShape{"Here As In Heaven - Rhythm", 4, "74", "Intro"}),
    [](const ::testing::TestParamInfo<SongShape>& i) { return sanitize(i.param.song); });

TEST(SongStructure, SuperSwitcherAlwaysHasNineLoopParams) {
    for (const std::string& s : allSongs()) {
        const Song& sg = song(s);
        for (const Part& part : sg.parts) {
            SCOPED_TRACE(s + "/" + part.name);
            const PedalState* ss = part.pedal("SuperSwitcher");
            ASSERT_NE(ss, nullptr);
            EXPECT_EQ(ss->params.size(), 9u);
        }
    }
}

TEST(SongStructure, EveryPartHasAllSixPedals) {
    for (const std::string& s : allSongs()) {
        const Song& sg = song(s);
        for (const Part& part : sg.parts) {
            SCOPED_TRACE(s + "/" + part.name);
            EXPECT_EQ(part.pedals.size(), 6u);
        }
    }
}
