#include "mc/domain/MenuTree.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

using namespace mc;

namespace {

// Builds: root(MidiController)
//   Setup -> [Sets, Songs(data list)]
//   Global -> [Knob Color]
//   Power
struct Fixture {
    MenuTree tree{"MidiController"};
    std::vector<std::string> msgs;
    std::string selected;

    MenuNode* setup = nullptr;
    MenuNode* global = nullptr;
    MenuNode* power = nullptr;
    MenuNode* songs = nullptr;

    Fixture() {
        tree.setSink([this](const std::string& m) { msgs.push_back(m); });
        tree.setRootMessage([] { return "Victory - Rhythm - 110BPM - Chorus"; });

        setup = tree.root()->addChild("Setup");
        global = tree.root()->addChild("Global");
        power = tree.root()->addChild("Power");
        tree.setAnchors(setup, global, power);

        setup->addChild("Sets");
        songs = setup->addChild(
            "Songs",
            // func: populate the data list on enter
            [this] {
                songs->dataItems = {"Victory - Rhythm", "Here As In Heaven - Lead"};
                songs->dataPrompt = "Songs:";
            },
            // dataFunc: the load/select action
            [this] { selected = songs->dataItems[songs->dataPosition]; });

        global->addChild("Knob Color");
    }
};

}  // namespace

TEST(MenuTree, ShortPressAtRootEntersSetup) {
    Fixture f;
    f.tree.handleShortPress();
    EXPECT_EQ(f.tree.current()->name, "Setup");
    EXPECT_EQ(f.tree.displayedMessage(), "Setup: - Sets");
}

TEST(MenuTree, EncoderScrollsChildren) {
    Fixture f;
    f.tree.handleShortPress();        // -> Setup, child 0 (Sets)
    f.tree.changeMenuPos("CW");       // -> child 1 (Songs)
    EXPECT_EQ(f.tree.displayedMessage(), "Setup: - Songs");
    f.tree.changeMenuPos("CW");       // clamped at last child
    EXPECT_EQ(f.tree.displayedMessage(), "Setup: - Songs");
    f.tree.changeMenuPos("CCW");      // -> back to Sets
    EXPECT_EQ(f.tree.displayedMessage(), "Setup: - Sets");
}

TEST(MenuTree, EnterDataListAndScrollAndSelect) {
    Fixture f;
    f.tree.handleShortPress();   // Setup
    f.tree.changeMenuPos("CW");  // Songs highlighted
    f.tree.handleShortPress();   // enter Songs -> func populates list
    EXPECT_EQ(f.tree.displayedMessage(), "Setup: - Songs: - Victory - Rhythm");
    f.tree.changeMenuPos("CW");  // next data item
    EXPECT_EQ(f.tree.displayedMessage(), "Setup: - Songs: - Here As In Heaven - Lead");
    f.tree.handleShortPress();   // select -> dataFunc
    EXPECT_EQ(f.selected, "Here As In Heaven - Lead");
}

TEST(MenuTree, LongPressGoesToParent) {
    Fixture f;
    f.tree.handleShortPress();   // Setup
    f.tree.changeMenuPos("CW");  // Songs
    f.tree.handleShortPress();   // into Songs data list
    f.tree.handleLongPress();    // back to Setup
    EXPECT_EQ(f.tree.current()->name, "Setup");
}

TEST(MenuTree, PressDurationTransitions) {
    Fixture f;
    f.tree.pressFor(0.2);  // short at root -> Setup
    EXPECT_EQ(f.tree.current()->name, "Setup");

    f.tree.changeMenuNodes(f.tree.root());
    f.tree.pressFor(3.0);  // 2..5s at root -> Global
    EXPECT_EQ(f.tree.current()->name, "Global");

    f.tree.pressFor(6.0);  // >5s -> Power
    EXPECT_EQ(f.tree.current()->name, "Power");

    f.tree.pressFor(1.0);  // long press -> parent (root)
    EXPECT_EQ(f.tree.current(), f.tree.root());
    EXPECT_EQ(f.tree.displayedMessage(), "Victory - Rhythm - 110BPM - Chorus");
}
