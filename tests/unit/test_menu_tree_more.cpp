// Additional MenuTree coverage: node helpers, clamping, deep message variants,
// error node, press-duration boundaries.
#include "gtest/gtest.h"
#include "mc/domain/MenuTree.h"

using namespace mc;

TEST(MenuNodeMore, AddChildSetsParentAndReturns) {
    MenuTree t("root");
    MenuNode* c = t.root()->addChild("Child");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->name, "Child");
    EXPECT_EQ(c->parent, t.root());
    EXPECT_EQ(t.root()->children.size(), 1u);
}

TEST(MenuNodeMore, CurrentChildNodeBounds) {
    MenuNode n;
    EXPECT_EQ(n.currentChildNode(), nullptr);  // no children
    n.addChild("a");
    n.addChild("b");
    n.currentChild = 1;
    EXPECT_EQ(n.currentChildNode()->name, "b");
    n.currentChild = 99;  // out of range -> first
    EXPECT_EQ(n.currentChildNode()->name, "a");
    n.currentChild = -1;  // negative -> first
    EXPECT_EQ(n.currentChildNode()->name, "a");
}

TEST(MenuTreeMore, ChangeMenuPosAtRootIsNoOp) {
    MenuTree t("root");
    std::string last = "unset";
    t.setSink([&](const std::string& m) { last = m; });
    t.changeMenuPos("CW");
    t.changeMenuPos("CCW");
    EXPECT_EQ(last, "unset");  // root has no list to scroll
}

TEST(MenuTreeMore, DataListClampsBothEnds) {
    MenuTree t("root");
    std::vector<std::string> msgs;
    t.setSink([&](const std::string& m) { msgs.push_back(m); });
    MenuNode* node = t.root()->addChild("List", [&] {
        t.current()->dataItems = {"a", "b", "c"};
        t.current()->dataPrompt = "L:";
    });
    t.changeMenuNodes(node);  // enter -> "L: - a"
    t.changeMenuPos("CCW");   // clamp at start, no change
    EXPECT_EQ(t.displayedMessage(), "L: - a");
    t.changeMenuPos("CW");    // -> b
    EXPECT_EQ(t.displayedMessage(), "L: - b");
    t.changeMenuPos("CW");    // -> c
    t.changeMenuPos("CW");    // clamp at end
    EXPECT_EQ(t.displayedMessage(), "L: - c");
}

TEST(MenuTreeMore, GrandparentMessageVariant) {
    MenuTree t("root");
    std::string last;
    t.setSink([&](const std::string& m) { last = m; });
    MenuNode* one = t.root()->addChild("One");
    MenuNode* two = one->addChild("Two");
    MenuNode* three = two->addChild("Three", [&] {
        three->dataItems = {"item"};
        three->dataPrompt = "P:";
    });
    t.changeMenuNodes(three);
    EXPECT_EQ(last, "One: - Two: - P: - item");
}

TEST(MenuTreeMore, ParentMessageVariant) {
    MenuTree t("root");
    std::string last;
    t.setSink([&](const std::string& m) { last = m; });
    MenuNode* a = t.root()->addChild("A");
    MenuNode* b = a->addChild("B", [&] {
        b->dataItems = {"x"};
        b->dataPrompt = "P:";
    });
    t.changeMenuNodes(b);
    EXPECT_EQ(last, "A: - P: - x");
}

TEST(MenuTreeMore, ChildMessageVariantAtTopLevel) {
    MenuTree t("root");
    std::string last;
    t.setSink([&](const std::string& m) { last = m; });
    MenuNode* a = t.root()->addChild("A", [&] {
        a->dataItems = {"x"};
        a->dataPrompt = "P:";
    });
    t.changeMenuNodes(a);
    EXPECT_EQ(last, "P: - x");  // parent is root -> no parent prefix
}

TEST(MenuTreeMore, LongPressAtRootStaysAtRoot) {
    MenuTree t("root");
    t.setRootMessage([] { return "ROOT"; });
    t.handleLongPress();  // root has no parent
    EXPECT_EQ(t.current(), t.root());
}

TEST(MenuTreeMore, EmptyNodeShowsError) {
    MenuTree t("root");
    std::string last;
    t.setSink([&](const std::string& m) { last = m; });
    MenuNode* bare = t.root()->addChild("Bare");  // no func, no children, no data
    t.changeMenuNodes(bare);
    EXPECT_EQ(last, "Error!!");
}

TEST(MenuTreeMore, ChangeMenuNodesNullGoesRoot) {
    MenuTree t("root");
    t.setRootMessage([] { return "ROOT MSG"; });
    MenuNode* a = t.root()->addChild("A");
    t.changeMenuNodes(a);
    t.changeMenuNodes();  // null -> root
    EXPECT_EQ(t.current(), t.root());
    EXPECT_EQ(t.displayedMessage(), "ROOT MSG");
}

struct PressBand {
    double seconds;
    std::string expectName;  // node name we expect to land on
    std::string id;
};
class MenuPress : public ::testing::TestWithParam<PressBand> {};
TEST_P(MenuPress, Transitions) {
    const auto& c = GetParam();
    MenuTree t("root");
    t.setRootMessage([] { return "R"; });
    MenuNode* setup = t.root()->addChild("Setup");
    MenuNode* global = t.root()->addChild("Global");
    MenuNode* power = t.root()->addChild("Power");
    t.setAnchors(setup, global, power);
    t.pressFor(c.seconds);
    EXPECT_EQ(t.current()->name, c.expectName);
}
INSTANTIATE_TEST_SUITE_P(FromRoot, MenuPress,
                         ::testing::Values(PressBand{0.2, "Setup", "short_to_setup"},
                                           PressBand{0.49, "Setup", "just_short"},
                                           PressBand{3.0, "Global", "mid_to_global"},
                                           PressBand{5.0, "Global", "five_to_global"},
                                           PressBand{6.0, "Power", "long_to_power"}),
                         [](const ::testing::TestParamInfo<PressBand>& i) { return i.param.id; });

TEST(MenuTreeMore, LongPressBandReturnsToParent) {
    MenuTree t("root");
    t.setRootMessage([] { return "R"; });
    MenuNode* setup = t.root()->addChild("Setup");
    t.setAnchors(setup, nullptr, nullptr);
    t.changeMenuNodes(setup);
    t.pressFor(1.0);  // 0.5..2 -> long press -> parent (root)
    EXPECT_EQ(t.current(), t.root());
}
