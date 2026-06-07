// Not-found / edge branches of the domain accessors (built in-memory, no files).
#include "gtest/gtest.h"
#include "mc/domain/ControllerState.h"
#include "mc/domain/SongPartSet.h"

using namespace mc;

namespace {
Song twoPartSong() {
    Song s;
    s.name = "S";
    s.bpm = "120";
    Part a;
    a.name = "A";
    a.pedals.push_back({"Pedal1", PedalState{}});
    Part b;
    b.name = "B";
    s.parts = {a, b};
    return s;
}
}  // namespace

TEST(Accessors, SongPartFoundAndMissing) {
    Song s = twoPartSong();
    EXPECT_NE(s.part("A"), nullptr);
    EXPECT_NE(s.part("B"), nullptr);
    EXPECT_EQ(s.part("Z"), nullptr);
    EXPECT_EQ(s.indexOfPart("A"), 0);
    EXPECT_EQ(s.indexOfPart("B"), 1);
    EXPECT_EQ(s.indexOfPart("Z"), -1);
}

TEST(Accessors, PartPedalFoundAndMissing) {
    Song s = twoPartSong();
    EXPECT_NE(s.part("A")->pedal("Pedal1"), nullptr);
    EXPECT_EQ(s.part("A")->pedal("Nope"), nullptr);
    EXPECT_EQ(s.part("B")->pedal("Pedal1"), nullptr);  // B has no pedals
}

TEST(Accessors, SongBpmValue) {
    Song s = twoPartSong();
    EXPECT_DOUBLE_EQ(s.bpmValue(), 120.0);
    Song empty;
    EXPECT_DOUBLE_EQ(empty.bpmValue(), 0.0);  // empty bpm -> 0
}

TEST(Accessors, SetlistSongFoundAndMissing) {
    Setlist sl;
    sl.name = "Set";
    Song one;
    one.name = "One";
    Song two;
    two.name = "Two";
    sl.songs = {one, two};
    EXPECT_NE(sl.song("One"), nullptr);
    EXPECT_EQ(sl.song("Three"), nullptr);
    EXPECT_EQ(sl.indexOfSong("Two"), 1);
    EXPECT_EQ(sl.indexOfSong("Three"), -1);
}

TEST(Accessors, ControllerChannelForPedal) {
    ControllerState st;
    st.channels.push_back(ChannelConfig{2, "TimeLine", true, Value{1L}});
    st.channels.push_back(ChannelConfig{16, "ScarlettLove", true, Value{std::string("Plexi")}});
    EXPECT_NE(st.channelForPedal("TimeLine"), nullptr);
    EXPECT_EQ(st.channelForPedal("TimeLine")->channel, 2);
    EXPECT_EQ(st.channelForPedal("Unknown"), nullptr);
}

TEST(Accessors, ControllerButtonLookup) {
    ControllerState st;
    st.buttons.push_back(ButtonConfig{1, "Part Dn", "Select Part Dn", ""});
    EXPECT_NE(st.button(1), nullptr);
    EXPECT_EQ(st.button(1)->function, "Part Dn");
    EXPECT_EQ(st.button(99), nullptr);
}
