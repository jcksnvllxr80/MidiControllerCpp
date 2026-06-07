// FsConfigStore round-trip (read/write/exists/list) in a temp directory.
#include <filesystem>

#include "gtest/gtest.h"
#include "mc/adapters/sim/FsConfigStore.h"

namespace fs = std::filesystem;
using namespace mc;

TEST(FsConfigStore, WriteReadExistsList) {
    fs::path base = fs::temp_directory_path() / "mc_fscs_test";
    fs::remove_all(base);

    sim::FsConfigStore store(base.string());
    EXPECT_FALSE(store.exists("sets/Foo.json"));

    store.write("sets/Foo.json", "{\"name\":\"Foo\"}");
    store.write("sets/Bar.json", "{\"name\":\"Bar\"}");
    store.write("midi_controller.json", "{}");

    EXPECT_TRUE(store.exists("sets/Foo.json"));
    EXPECT_EQ(store.read("sets/Foo.json"), "{\"name\":\"Foo\"}");

    auto sets = store.list("sets");
    ASSERT_EQ(sets.size(), 2u);
    EXPECT_EQ(sets[0], "Bar");  // sorted
    EXPECT_EQ(sets[1], "Foo");

    EXPECT_THROW(store.read("sets/Missing.json"), std::runtime_error);

    fs::remove_all(base);
}
