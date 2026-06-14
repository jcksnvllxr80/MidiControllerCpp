// McuConfigStore tombstone/overlay logic. McuConfigStore.cpp has NO Pico-SDK
// dependencies, but the Makefile excludes src/adapters/mcu/* from the desktop
// build, so we pull the TU in directly to exercise it on the host. (Safe: it's
// compiled in exactly one place — here — so there are no duplicate symbols.)
#include "../../src/adapters/mcu/McuConfigStore.cpp"

#include <stdexcept>

#include "gtest/gtest.h"

using namespace mc::mcu;

namespace {
struct FakeKv : IKeyValueStore {
    std::map<std::string, std::string> data;
    std::map<std::string, std::string> load() override { return data; }
    void save(const std::map<std::string, std::string>& kv) override { data = kv; }
};
const EmbeddedBlob kTable[] = {{"sets/A.json", "A", 1}, {"sets/B.json", "B", 1}};
}  // namespace

TEST(McuConfigStore, RemoveTombstonesEmbeddedAndSurvivesReload) {
    FakeKv kv;
    {
        McuConfigStore s(kTable, 2, &kv);
        EXPECT_TRUE(s.exists("sets/A.json"));
        s.remove("sets/A.json");  // can't erase the flash blob -> tombstone it
        EXPECT_FALSE(s.exists("sets/A.json"));
        EXPECT_THROW(s.read("sets/A.json"), std::runtime_error);
        ASSERT_EQ(s.list("sets").size(), 1u);
        EXPECT_EQ(s.list("sets")[0], "B");
    }
    // A fresh instance reloads the overlay (including the tombstone) from flash.
    McuConfigStore s2(kTable, 2, &kv);
    EXPECT_FALSE(s2.exists("sets/A.json"));
    EXPECT_EQ(s2.list("sets").size(), 1u);

    // Writing the key again resurrects it (clears the tombstone).
    s2.write("sets/A.json", "A2");
    EXPECT_TRUE(s2.exists("sets/A.json"));
    EXPECT_EQ(s2.read("sets/A.json"), "A2");
    EXPECT_EQ(s2.list("sets").size(), 2u);
}

TEST(McuConfigStore, RemoveOverlayOnlyKeyJustDropsIt) {
    FakeKv kv;
    McuConfigStore s(nullptr, 0, &kv);
    s.write("songs/X.json", "{}");
    ASSERT_TRUE(s.exists("songs/X.json"));
    s.remove("songs/X.json");
    EXPECT_FALSE(s.exists("songs/X.json"));
    EXPECT_TRUE(s.list("songs").empty());
    // No tombstone was stored for a key with no embedded blob behind it.
    EXPECT_TRUE(kv.data.find("songs/X.json") == kv.data.end());
}
