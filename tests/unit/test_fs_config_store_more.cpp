// Additional FsConfigStore behaviors against a temp directory.
#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"
#include "mc/adapters/sim/FsConfigStore.h"

namespace fs = std::filesystem;
using namespace mc;

namespace {
fs::path freshBase(const char* name) {
    fs::path base = fs::temp_directory_path() / name;
    fs::remove_all(base);
    return base;
}
}  // namespace

TEST(FsStoreMore, WriteCreatesNestedDirs) {
    auto base = freshBase("mc_fs_nested");
    sim::FsConfigStore store(base.string());
    store.write("a/b/c/deep.json", "{}");
    EXPECT_TRUE(fs::exists(base / "a" / "b" / "c" / "deep.json"));
    EXPECT_EQ(store.read("a/b/c/deep.json"), "{}");
    fs::remove_all(base);
}

TEST(FsStoreMore, OverwriteReplacesContent) {
    auto base = freshBase("mc_fs_overwrite");
    sim::FsConfigStore store(base.string());
    store.write("k.json", "first");
    store.write("k.json", "second");
    EXPECT_EQ(store.read("k.json"), "second");
    fs::remove_all(base);
}

TEST(FsStoreMore, ListIgnoresNonJsonAndSorts) {
    auto base = freshBase("mc_fs_list");
    sim::FsConfigStore store(base.string());
    store.write("d/zeta.json", "{}");
    store.write("d/alpha.json", "{}");
    // a non-json file should be ignored
    fs::create_directories(base / "d");
    std::ofstream(base / "d" / "notes.txt") << "ignore me";
    auto items = store.list("d");
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0], "alpha");
    EXPECT_EQ(items[1], "zeta");
    fs::remove_all(base);
}

TEST(FsStoreMore, ListMissingDirReturnsEmpty) {
    auto base = freshBase("mc_fs_missing");
    sim::FsConfigStore store(base.string());
    EXPECT_TRUE(store.list("nope").empty());
    fs::remove_all(base);
}

TEST(FsStoreMore, ExistsReflectsWrites) {
    auto base = freshBase("mc_fs_exists");
    sim::FsConfigStore store(base.string());
    EXPECT_FALSE(store.exists("x.json"));
    store.write("x.json", "{}");
    EXPECT_TRUE(store.exists("x.json"));
    fs::remove_all(base);
}

TEST(FsStoreMore, ReadsRealDataDir) {
    // The committed fixtures are reachable through the store too.
    sim::FsConfigStore store("data");
    EXPECT_TRUE(store.exists("midi_controller.json"));
    auto sets = store.list("sets");
    ASSERT_EQ(sets.size(), 3u);
    EXPECT_EQ(sets[0], "2016-09-16 Set");
    auto pedals = store.list("pedals");
    EXPECT_EQ(pedals.size(), 6u);
}
