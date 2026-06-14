// EditorProtocol against the real fixtures — verifies the firmware speaks the
// exact wire format the controller app (MidiControllerControllerApp) expects.
#include <algorithm>
#include <filesystem>
#include <vector>

#include <nlohmann/json.hpp>

#include "gtest/gtest.h"
#include "mc/adapters/sim/FsConfigStore.h"
#include "mc/adapters/sim/SimAdapters.h"
#include "mc/app/Application.h"
#include "mc/app/EditorProtocol.h"
#include "support/FakeClock.h"
#include "support/RecordingMidiOut.h"
#include "support/RecordingPorts.h"

namespace fs = std::filesystem;
using namespace mc;
using namespace mc::test;
using json = nlohmann::json;

namespace {
struct Rig {
    sim::FsConfigStore store{"data"};
    RecordingMidiOut midi;
    RecordingTempoOut tempo;
    RecordingDisplay display;
    NullLed led;
    FakeClock clock;
    sim::ScriptedInput input;
    sim::NullConfigTransport tr;
    Application app;
    EditorProtocol proto;
    Rig() : app({&store, &midi, &tempo, &display, &led, &clock, &input, &tr}), proto(store, app) {
        app.setup();
    }
    json resp(const std::string& line) {
        std::string r = proto.handleLine(line);
        return r.empty() ? json() : json::parse(r);
    }
};
}  // namespace

TEST(Protocol, IdentifyReturnsDeviceIdentity) {
    Rig r;
    auto resp = r.resp(R"({"id":3,"op":"identify"})");
    EXPECT_EQ(resp["id"], 3);
    EXPECT_EQ(resp["ok"], true);
    EXPECT_EQ(resp["data"]["name"], "MidiController");
    EXPECT_TRUE(resp["data"]["firmware"].is_string());
    EXPECT_TRUE(resp["data"]["protocol_version"].is_number());  // DeviceIdentity shape
}

TEST(Protocol, ResponseIsSingleNewlineTerminatedLine) {
    Rig r;
    std::string raw = r.proto.handleLine(R"({"id":1,"op":"ping"})");
    ASSERT_FALSE(raw.empty());
    EXPECT_EQ(raw.back(), '\n');
    EXPECT_EQ(std::count(raw.begin(), raw.end(), '\n'), 1);
}

TEST(Protocol, PingOk) {
    Rig r;
    EXPECT_EQ(r.resp(R"({"id":7,"op":"ping"})")["ok"], true);
}

TEST(Protocol, IdIsEchoed) {
    Rig r;
    EXPECT_EQ(r.resp(R"({"id":12345,"op":"ping"})")["id"], 12345);
}

TEST(Protocol, ListSetsContainsRealSets) {
    Rig r;
    auto data = r.resp(R"({"id":1,"op":"list_sets"})")["data"];
    ASSERT_TRUE(data.is_array());
    std::vector<std::string> names = data;
    EXPECT_NE(std::find(names.begin(), names.end(), "2017-05-22 Set"), names.end());
}

TEST(Protocol, ListPedalsAndSongs) {
    Rig r;
    std::vector<std::string> pedals = r.resp(R"({"id":1,"op":"list_pedals"})")["data"];
    EXPECT_EQ(pedals.size(), 6u);
    std::vector<std::string> songs = r.resp(R"({"id":1,"op":"list_songs"})")["data"];
    EXPECT_EQ(songs.size(), 4u);
}

TEST(Protocol, GetPedalReturnsConfigObject) {
    Rig r;
    auto resp = r.resp(R"({"id":2,"op":"get_pedal","name":"TimeLine"})");
    EXPECT_EQ(resp["ok"], true);
    EXPECT_EQ(resp["data"]["Engage"]["cc"], 102);
}

TEST(Protocol, GetSongReturnsSong) {
    Rig r;
    auto resp = r.resp(R"({"id":2,"op":"get_song","name":"Victory - Rhythm"})");
    EXPECT_EQ(resp["data"]["name"], "Victory - Rhythm");
    EXPECT_EQ(resp["data"]["tempo"], 110);
}

TEST(Protocol, DpadDownEntersMenu) {
    Rig r;
    auto resp = r.resp(R"({"id":1,"op":"dpad","direction":"down"})");
    EXPECT_EQ(resp["ok"], true);
    EXPECT_EQ(resp["data"]["display_message"], "Setup: - Sets");
}

TEST(Protocol, ShortPreviewThenSelectLoadsAndReportsDisplay) {
    Rig r;
    auto prev = r.resp(R"({"id":1,"op":"short","button":"5"})");  // Song Up (preview)
    EXPECT_EQ(prev["data"]["display_message"], "Here As In Heaven - Lead - 74BPM - Intro");
    EXPECT_TRUE(r.midi.messages.empty());  // preview emits no MIDI

    auto sel = r.resp(R"({"id":2,"op":"short","button":"2"})");  // Select -> load
    EXPECT_EQ(r.app.currentSongName(), "Here As In Heaven - Lead");
    EXPECT_FALSE(r.midi.messages.empty());
    EXPECT_EQ(sel["data"]["display_message"], "Here As In Heaven - Lead - 74BPM - Intro");
}

TEST(Protocol, NoiseLineProducesNoResponse) {
    Rig r;
    EXPECT_EQ(r.proto.handleLine("boot: hello world"), "");
    EXPECT_EQ(r.proto.handleLine(""), "");
    EXPECT_EQ(r.proto.handleLine("   "), "");
    EXPECT_EQ(r.proto.handleLine("[1,2,3]"), "");  // JSON but not an object
}

TEST(Protocol, UnknownOpIsError) {
    Rig r;
    auto resp = r.resp(R"({"id":9,"op":"frobnicate"})");
    EXPECT_EQ(resp["id"], 9);
    EXPECT_EQ(resp["ok"], false);
    EXPECT_NE(std::string(resp["error"]).find("unsupported"), std::string::npos);
}

TEST(Protocol, MissingNameIsErrorNotCrash) {
    Rig r;
    auto resp = r.resp(R"({"id":4,"op":"get_pedal"})");  // no "name"
    EXPECT_EQ(resp["ok"], false);
    EXPECT_TRUE(resp["error"].is_string());
}

TEST(Protocol, WritePedalPersistsToStore) {
    // Use a temp store so the committed fixtures aren't touched.
    fs::path base = fs::temp_directory_path() / "mc_proto_write";
    fs::remove_all(base);
    sim::FsConfigStore tmp(base.string());
    Rig r;  // for the Application (unused by write ops)
    EditorProtocol p(tmp, r.app);

    auto resp = json::parse(p.handleLine(R"({"id":1,"op":"write_pedal","name":"X","data":{"Engage":{"cc":7}}})"));
    EXPECT_EQ(resp["ok"], true);
    ASSERT_TRUE(tmp.exists("pedals/X.json"));
    EXPECT_EQ(json::parse(tmp.read("pedals/X.json"))["Engage"]["cc"], 7);
    fs::remove_all(base);
}

TEST(Protocol, WifiUnsupportedWhenNoRadio) {
    Rig r;  // EditorProtocol constructed without an IWifi
    auto resp = r.resp(R"({"id":1,"op":"wifi_status"})");
    EXPECT_EQ(resp["ok"], false);
    EXPECT_NE(std::string(resp["error"]).find("wifi"), std::string::npos);
}

namespace {
struct FakeWifi : IWifi {
    WifiStatus st;
    std::string lastPass;
    void setCredentials(const std::string& ssid, const std::string& pass) override {
        st.ssid = ssid;
        lastPass = pass;
        st.connected = true;
        st.ip = "192.168.1.50";
    }
    void setEnabled(bool on) override {
        st.enabled = on;
        if (!on) {
            st.connected = false;
            st.ip = "";
        }
    }
    WifiStatus status() const override { return st; }
};
}  // namespace

TEST(Protocol, WifiSetEnableStatus) {
    sim::FsConfigStore store{"data"};
    RecordingMidiOut midi;
    RecordingTempoOut tempo;
    RecordingDisplay display;
    NullLed led;
    FakeClock clock;
    sim::ScriptedInput input;
    sim::NullConfigTransport tr;
    Application app({&store, &midi, &tempo, &display, &led, &clock, &input, &tr});
    app.setup();
    FakeWifi wifi;
    EditorProtocol proto(store, app, &wifi);

    auto set = json::parse(proto.handleLine(R"({"id":1,"op":"wifi_set","ssid":"Net","password":"pw"})"));
    EXPECT_EQ(set["ok"], true);
    EXPECT_EQ(set["data"]["enabled"], true);
    EXPECT_EQ(set["data"]["connected"], true);
    EXPECT_EQ(set["data"]["ssid"], "Net");
    EXPECT_EQ(set["data"]["ip"], "192.168.1.50");
    EXPECT_EQ(wifi.lastPass, "pw");

    auto off = json::parse(proto.handleLine(R"({"id":2,"op":"wifi_enable","on":false})"));
    EXPECT_EQ(off["data"]["enabled"], false);
    EXPECT_EQ(off["data"]["connected"], false);

    auto st = json::parse(proto.handleLine(R"({"id":3,"op":"wifi_status"})"));
    EXPECT_EQ(st["ok"], true);
    EXPECT_EQ(st["data"]["enabled"], false);
}
