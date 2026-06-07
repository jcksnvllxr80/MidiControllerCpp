// ConfigLoader internals exercised through crafted JSON strings.
#include <stdexcept>
#include <string>

#include "gtest/gtest.h"
#include "mc/config/ConfigLoader.h"

using namespace mc;
using namespace mc::config;

// ---- pedal config parsing ---------------------------------------------------

TEST(LoaderPedal, LeafKeys) {
    auto c = loadPedalConfigFromString(R"({"Engage":{"cc":102,"value":127},"Bypass":{"cc":102,"value":0}})");
    ASSERT_NE(c.engage(), nullptr);
    EXPECT_EQ(c.engage()->cc.value_or(-1), 102);
    EXPECT_EQ(c.engage()->value.value_or(-1), 127);
    EXPECT_EQ(c.bypass()->value.value_or(-1), 0);
}

TEST(LoaderPedal, MultiOrderedByNumericKey) {
    auto c = loadPedalConfigFromString(
        R"({"Set Preset":{"multi":{"2":"second","1":"first"},"first":{"cc":5},"second":{"cc":6}}})");
    const Action* sp = c.setPreset();
    ASSERT_NE(sp, nullptr);
    ASSERT_EQ(sp->multi.size(), 2u);
    EXPECT_EQ(sp->multi[0], "first");   // key "1" sorts before "2"
    EXPECT_EQ(sp->multi[1], "second");
}

TEST(LoaderPedal, DictOrderPreserved) {
    auto c = loadPedalConfigFromString(R"({"Knobs/Switches":{"Type":{"cc":1,"dict":{"z":0,"a":1,"m":2}}}})");
    const Action* t = c.paramGroup("Knobs/Switches")->child("Type");
    ASSERT_NE(t, nullptr);
    ASSERT_EQ(t->dict.size(), 3u);
    EXPECT_EQ(t->dict[0].first, "z");  // insertion order, not alphabetical
    EXPECT_EQ(t->dict[1].first, "a");
    EXPECT_EQ(t->dict[2].first, "m");
}

TEST(LoaderPedal, ProgramAndControlChangeSubActions) {
    auto c = loadPedalConfigFromString(
        R"JSON({"A":{"program change":{"func":"x % 128"}},"B":{"control change":{"func":"{\"k\":3}.get(x, None)","options":["k"]}}})JSON");
    const Action* a = c.group("A");
    ASSERT_TRUE(a->programChange.has_value());
    EXPECT_EQ(a->programChange->transform.op(), '%');
    const Action* b = c.group("B");
    ASSERT_TRUE(b->controlChange.has_value());
    EXPECT_EQ(b->controlChange->transform.kind(), Transform::Kind::Lookup);
    EXPECT_EQ(b->controlChange->options.size(), 1u);
}

TEST(LoaderPedal, NotesAndDisplayIgnoredNestedChildrenKept) {
    auto c = loadPedalConfigFromString(
        R"({"G":{"notes":"hi","display":{"page":1},"Child":{"cc":7}}})");
    const Action* g = c.group("G");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->child("notes"), nullptr);    // metadata, not a child
    EXPECT_EQ(g->child("display"), nullptr);
    ASSERT_NE(g->child("Child"), nullptr);
    EXPECT_EQ(g->child("Child")->cc.value_or(-1), 7);
}

TEST(LoaderPedal, BadFuncThrows) {
    EXPECT_THROW(loadPedalConfigFromString(R"({"A":{"cc":1,"func":"x ** 2"}})"), std::runtime_error);
}

// ---- song parsing -----------------------------------------------------------

TEST(LoaderSong, TempoIntBecomesString) {
    auto s = loadSongFromString(R"({"name":"S","tempo":110,"parts":{}})");
    EXPECT_EQ(s.name, "S");
    EXPECT_EQ(s.bpm, "110");
}
TEST(LoaderSong, TempoStringKept) {
    auto s = loadSongFromString(R"({"name":"S","tempo":"96","parts":{}})");
    EXPECT_EQ(s.bpm, "96");
}
TEST(LoaderSong, NameFallback) {
    auto s = loadSongFromString(R"({"tempo":100,"parts":{}})", "Fallback");
    EXPECT_EQ(s.name, "Fallback");
}
TEST(LoaderSong, PartsOrderedByPosition) {
    auto s = loadSongFromString(
        R"({"name":"S","tempo":1,"parts":{"B":{"position":2,"pedals":{}},"A":{"position":1,"pedals":{}}}})");
    ASSERT_EQ(s.parts.size(), 2u);
    EXPECT_EQ(s.parts[0].name, "A");
    EXPECT_EQ(s.parts[1].name, "B");
}
TEST(LoaderSong, EngagedDefaultsFalsePresetDefaultsEmpty) {
    auto s = loadSongFromString(R"({"name":"S","tempo":1,"parts":{"P":{"position":1,"pedals":{"X":{}}}}})");
    const PedalState* x = s.part("P")->pedal("X");
    ASSERT_NE(x, nullptr);
    EXPECT_FALSE(x->engaged);
    EXPECT_EQ(x->preset, Value{std::string("")});
    EXPECT_TRUE(x->params.empty());
}
TEST(LoaderSong, NullParamsBecomeEmpty) {
    auto s = loadSongFromString(R"({"name":"S","tempo":1,"parts":{"P":{"position":1,"pedals":{"X":{"params":null}}}}})");
    EXPECT_TRUE(s.part("P")->pedal("X")->params.empty());
}
TEST(LoaderSong, EngagedRefParamValue) {
    auto s = loadSongFromString(
        R"({"name":"S","tempo":1,"parts":{"P":{"position":1,"pedals":{"X":{"params":{"L":{"name":"n","engaged":true}}}}}}})");
    const PedalState* x = s.part("P")->pedal("X");
    ASSERT_EQ(x->params.size(), 1u);
    const Value want = EngagedRef{"n", true};
    EXPECT_EQ(x->params[0].second, want);
}
TEST(LoaderSong, IntAndStringPresets) {
    auto s = loadSongFromString(
        R"({"name":"S","tempo":1,"parts":{"P":{"position":1,"pedals":{"I":{"preset":3},"N":{"preset":"Plexi"}}}}})");
    EXPECT_EQ(s.part("P")->pedal("I")->preset, Value{3L});
    EXPECT_EQ(s.part("P")->pedal("N")->preset, Value{std::string("Plexi")});
}

// ---- controller state -------------------------------------------------------

TEST(LoaderController, CoreFields) {
    auto st = loadControllerStateFromString(R"({
        "controller_api":{"buttons_locked":true,"port":9000},
        "current_settings":{"mode":"favorite","tempo":120,"preset":{"setList":"S","song":"So","part":"Pa"}},
        "knob":{"color":"Cyan","brightness":"50"}
    })");
    EXPECT_TRUE(st.buttonsLocked);
    EXPECT_EQ(st.apiPort, 9000);
    EXPECT_EQ(st.mode, "favorite");
    EXPECT_EQ(st.tempo, 120);
    EXPECT_EQ(st.currentSet, "S");
    EXPECT_EQ(st.currentSong, "So");
    EXPECT_EQ(st.currentPart, "Pa");
    EXPECT_EQ(st.knobColor, "Cyan");
    EXPECT_EQ(st.knobBrightness, 50);
}

TEST(LoaderController, NullChannelsDroppedAndOrdered) {
    auto st = loadControllerStateFromString(R"({
        "midi":{"channels":{"3":{"name":"C","state":false,"preset":{"name":"P"}},
                            "1":{"name":"A","state":true,"preset":{"number":2}},
                            "2":null}}
    })");
    ASSERT_EQ(st.channels.size(), 2u);
    EXPECT_EQ(st.channels[0].channel, 1);
    EXPECT_EQ(st.channels[0].name, "A");
    EXPECT_EQ(st.channels[0].preset, Value{2L});       // number
    EXPECT_EQ(st.channels[1].channel, 3);
    EXPECT_EQ(st.channels[1].preset, Value{std::string("P")});  // name
}

TEST(LoaderController, BrightnessAsIntAlsoWorks) {
    auto st = loadControllerStateFromString(R"({"knob":{"color":"Red","brightness":70}})");
    EXPECT_EQ(st.knobBrightness, 70);
}

TEST(LoaderController, ButtonSetupWithAndWithoutLongPress) {
    auto st = loadControllerStateFromString(R"({
        "button_setup":{"1":{"function":"Part Dn","long_press_func":"Select Part Dn"},
                        "2":{"function":"Select"}}
    })");
    ASSERT_EQ(st.buttons.size(), 2u);
    EXPECT_EQ(st.button(1)->function, "Part Dn");
    EXPECT_EQ(st.button(1)->longPressFunc, "Select Part Dn");
    EXPECT_EQ(st.button(2)->function, "Select");
    EXPECT_EQ(st.button(2)->longPressFunc, "");
}

// ---- setlist + file io ------------------------------------------------------

TEST(LoaderSetlist, FromStringUsesFetchCallback) {
    auto sl = loadSetlistFromString(R"({"name":"Set","songs":["One","Two"]})", [](const std::string& n) {
        return std::string(R"({"name":")") + n + R"(","tempo":80,"parts":{}})";
    });
    EXPECT_EQ(sl.name, "Set");
    ASSERT_EQ(sl.songs.size(), 2u);
    EXPECT_EQ(sl.songs[0].name, "One");
    EXPECT_EQ(sl.songs[1].name, "Two");
}

TEST(LoaderFile, ReadMissingThrows) {
    EXPECT_THROW(readFile("does/not/exist.json"), std::runtime_error);
}
TEST(LoaderFile, ReadRealFile) {
    EXPECT_FALSE(readFile("data/midi_controller.json").empty());
}
