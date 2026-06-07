#pragma once
//
// MenuTree — the rotary-knob menu. Ports N_Tree.py (the tree) plus the
// navigation engine from RotaryEncoder.py (change_menu_pos / handle_short_press
// / handle_long_press / change_menu_nodes and the press-duration transitions of
// RotaryPushButton.button_state).
//
// It is deliberately hardware-free: messages go out through an injected sink
// (the Application points it at IDisplay), and node behaviours are std::function
// callbacks the Application wires up (load a set, change knob colour, etc.). That
// keeps the messy, device-specific actions out of the tree so the navigation
// itself is unit-testable.
//
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace mc {

class MenuNode {
public:
    std::string name;
    MenuNode* parent = nullptr;
    std::vector<std::unique_ptr<MenuNode>> children;
    int currentChild = 0;

    std::function<void()> func;      // "show_*" — populate dataItems / act on enter
    std::function<void()> dataFunc;  // "menu_data_func" — the load/select action

    std::vector<std::string> dataItems;
    int dataPosition = 0;
    std::string dataPrompt;
    std::map<std::string, std::function<void()>> dataDict;  // item -> action

    MenuNode* addChild(const std::string& childName, std::function<void()> nodeFunc = {},
                       std::function<void()> nodeDataFunc = {});
    bool hasChildren() const { return !children.empty(); }
    MenuNode* currentChildNode() const;
};

class MenuTree {
public:
    explicit MenuTree(const std::string& rootName);

    MenuNode* root() const { return root_.get(); }
    MenuNode* current() const { return current_; }
    const std::string& displayedMessage() const { return lastMessage_; }

    void setSink(std::function<void(const std::string&)> sink) { sink_ = std::move(sink); }
    void setRootMessage(std::function<std::string()> fn) { rootMessage_ = std::move(fn); }
    void setAnchors(MenuNode* setup, MenuNode* global, MenuNode* power);

    // rotary encoder turned
    void changeMenuPos(const std::string& direction);  // "CW" | "CCW"
    // rotary push button
    void handleShortPress();
    void handleLongPress();
    void pressFor(double seconds);  // classify by hold time -> menu transition
    void changeMenuNodes(MenuNode* node = nullptr);

private:
    void emit(const std::string& msg);
    void setChildrenMessage();
    void setMenuDataMessage();
    void nextMenuListItem();
    void prevMenuListItem();

    std::unique_ptr<MenuNode> root_;
    MenuNode* current_;
    MenuNode* setup_ = nullptr;
    MenuNode* global_ = nullptr;
    MenuNode* power_ = nullptr;
    std::function<void(const std::string&)> sink_;
    std::function<std::string()> rootMessage_;
    std::string lastMessage_;
};

}  // namespace mc
