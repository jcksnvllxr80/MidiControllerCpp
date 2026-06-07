// Drive the Application to every (song, part) in the real setlist and assert
// integration invariants: a load emits MIDI, is idempotent on reload, processes
// pedals in channel order (QuartzV2 first, ScarlettLove last), and pushes the
// song's tempo. Plus a second hand-verified golden (Here As In Heaven / Intro).
#include <map>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mc/adapters/sim/FsConfigStore.h"
#include "mc/adapters/sim/SimAdapters.h"
#include "mc/app/Application.h"
#include "mc/config/ConfigLoader.h"
#include "support/FakeClock.h"
#include "support/Fixtures.h"
#include "support/RecordingMidiOut.h"
#include "support/RecordingPorts.h"

using namespace mc;
using namespace mc::test;
using Bytes = std::vector<uint8_t>;
using Seq = std::vector<std::vector<uint8_t>>;

namespace {

struct Rig {
    sim::FsConfigStore store{"data"};
    RecordingMidiOut midi;
    RecordingTempoOut tempo;
    RecordingDisplay display;
    NullLed led;
    FakeClock clock;
    sim::ScriptedInput input;
    sim::NullConfigTransport transport;
    Application app;
    Rig() : app({&store, &midi, &tempo, &display, &led, &clock, &input, &transport}) { app.setup(); }
    void fsShort(int b) { app.handleEvent({InputEvent::Type::FootswitchShort, b, 0}); }
};

// Navigate (from the boot song/part) to a target and commit it.
void goTo(Rig& r, const std::string& song, const std::string& part) {
    for (int i = 0; i < 12 && r.app.displayedSongName() != song; ++i) r.fsShort(5);  // Song Up
    r.fsShort(2);                                                                     // Select
    for (int i = 0; i < 12 && r.app.displayedPartName() != part; ++i) r.fsShort(3);   // Part Up
    r.fsShort(2);                                                                     // Select
}

struct PartRef {
    std::string song;
    int partIdx;
    std::string part;
    std::string id;
};

std::vector<PartRef> genParts() {
    std::vector<PartRef> out;
    for (const std::string& s : allSongs()) {
        Song sg = config::loadSongFromFile("data/songs/" + s + ".json");
        for (size_t i = 0; i < sg.parts.size(); ++i)
            out.push_back({s, static_cast<int>(i), sg.parts[i].name, sanitize(s) + "_" + sanitize(sg.parts[i].name)});
    }
    return out;
}

double expectedBpm(const std::string& song) { return song.rfind("Victory", 0) == 0 ? 110.0 : 74.0; }

}  // namespace

class PartLoad : public ::testing::TestWithParam<PartRef> {};

TEST_P(PartLoad, IntegrationInvariants) {
    const auto& c = GetParam();
    Rig r;
    goTo(r, c.song, c.part);
    ASSERT_EQ(r.app.currentSongName(), c.song);
    ASSERT_EQ(r.app.currentPartName(), c.part);

    r.midi.clear();
    r.tempo.bpms.clear();
    r.app.loadPart();
    const Seq first = r.midi.byteSeq();

    // 1) a load emits MIDI
    ASSERT_FALSE(first.empty());
    // 2) tempo pushed
    ASSERT_FALSE(r.tempo.bpms.empty());
    EXPECT_DOUBLE_EQ(r.tempo.bpms.back(), expectedBpm(c.song));
    // 3) channel order: first message is QuartzV2 (ch1); ScarlettLove (ch16) is last group
    EXPECT_EQ(r.midi.messages.front().channel(), 1);
    int lastNon16 = -1, firstCh16 = static_cast<int>(r.midi.messages.size());
    for (int i = 0; i < static_cast<int>(r.midi.messages.size()); ++i) {
        int ch = r.midi.messages[i].channel();
        if (ch == 16) firstCh16 = std::min(firstCh16, i);
        else lastNon16 = std::max(lastNon16, i);
    }
    EXPECT_LT(lastNon16, firstCh16) << "ScarlettLove (ch16) messages must come last";

    // 4) idempotent: reloading the same part yields identical bytes
    r.midi.clear();
    r.app.loadPart();
    EXPECT_EQ(r.midi.byteSeq(), first);
}

INSTANTIATE_TEST_SUITE_P(EveryPart, PartLoad, ::testing::ValuesIn(genParts()),
                         [](const ::testing::TestParamInfo<PartRef>& i) { return i.param.id; });

TEST(E2EGolden, HereAsInHeavenLeadIntro) {
    Rig r;
    goTo(r, "Here As In Heaven - Lead", "Intro");
    r.midi.clear();
    r.app.loadPart();
    const Seq expected = {
        {0xB0, 0x61, 0x01},                                            // QuartzV2 preset 1 (no engage)
        {0xB1, 0x66, 0x00}, {0xC1, 0x00},                              // TimeLine bypass + preset 0
        {0xB2, 0x66, 0x7F}, {0xC2, 0x01},                              // Iridium engage + preset 1
        {0xB3, 0x66, 0x00}, {0xC3, 0x00},                              // BigSky bypass + preset 0
        {0xC4, 0x01},                                                  // SuperSwitcher preset 1
        {0xB4, 0x66, 0x00}, {0xB4, 0x67, 0x00}, {0xB4, 0x68, 0x00},    // loops off...
        {0xB4, 0x69, 0x00}, {0xB4, 0x6B, 0x00}, {0xB4, 0x6D, 0x00},
        {0xB4, 0x6C, 0x00}, {0xB4, 0x70, 0x00}, {0xB4, 0x71, 0x00},
        {0xBF, 0x17, 0x01}, {0xBF, 0x14, 0x7F},                        // ScarlettLove engage + TS808
    };
    EXPECT_EQ(r.midi.byteSeq(), expected);
}
