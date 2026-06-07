#include "mc/domain/MenuTree.h"

namespace mc {

MenuNode* MenuNode::addChild(const std::string& childName, std::function<void()> nodeFunc,
                             std::function<void()> nodeDataFunc) {
    auto child = std::make_unique<MenuNode>();
    child->name = childName;
    child->parent = this;
    child->func = std::move(nodeFunc);
    child->dataFunc = std::move(nodeDataFunc);
    children.push_back(std::move(child));
    return children.back().get();
}

MenuNode* MenuNode::currentChildNode() const {
    if (children.empty()) return nullptr;
    int idx = (currentChild >= 0 && currentChild < static_cast<int>(children.size())) ? currentChild : 0;
    return children[idx].get();
}

MenuTree::MenuTree(const std::string& rootName) {
    root_ = std::make_unique<MenuNode>();
    root_->name = rootName;
    current_ = root_.get();
}

void MenuTree::setAnchors(MenuNode* setup, MenuNode* global, MenuNode* power) {
    setup_ = setup;
    global_ = global;
    power_ = power;
}

void MenuTree::emit(const std::string& msg) {
    lastMessage_ = msg;
    if (sink_) sink_(msg);
}

// --- navigation: encoder turns ----------------------------------------------

void MenuTree::changeMenuPos(const std::string& direction) {
    if (current_ == root_.get()) return;  // root: song-info screen, no list nav
    if (current_->hasChildren()) {
        if (direction == "CW") {
            if (current_->currentChild < static_cast<int>(current_->children.size()) - 1) {
                ++current_->currentChild;
                setChildrenMessage();
            }
        } else if (direction == "CCW") {
            if (current_->currentChild > 0) {
                --current_->currentChild;
                setChildrenMessage();
            }
        }
    } else {
        if (direction == "CW") nextMenuListItem();
        else if (direction == "CCW") prevMenuListItem();
    }
}

void MenuTree::nextMenuListItem() {
    if (current_->dataPosition < static_cast<int>(current_->dataItems.size()) - 1) {
        ++current_->dataPosition;
        setMenuDataMessage();
    }
}

void MenuTree::prevMenuListItem() {
    if (current_->dataPosition > 0) {
        --current_->dataPosition;
        setMenuDataMessage();
    }
}

// --- navigation: push button -------------------------------------------------

void MenuTree::handleShortPress() {
    if (current_ == root_.get()) {
        if (setup_) changeMenuNodes(setup_);
    } else if (current_->hasChildren()) {
        changeMenuNodes(current_->currentChildNode());
    } else if (!current_->dataItems.empty()) {
        if (current_->dataFunc) {
            current_->dataFunc();
        } else {
            const std::string& item = current_->dataItems[current_->dataPosition];
            auto it = current_->dataDict.find(item);
            if (it != current_->dataDict.end() && it->second) it->second();
        }
    }
}

void MenuTree::handleLongPress() {
    if (current_->parent) changeMenuNodes(current_->parent);
}

void MenuTree::pressFor(double seconds) {
    if (seconds < 0.5) {
        handleShortPress();
    } else if (seconds < 2.0) {
        handleLongPress();
    } else if (seconds > 5.0) {
        if (power_ && current_ != power_) changeMenuNodes(power_);
    } else if (current_ == root_.get()) {
        if (global_) changeMenuNodes(global_);
    } else {
        changeMenuNodes(root_.get());
    }
}

void MenuTree::changeMenuNodes(MenuNode* node) {
    if (!node) node = root_.get();
    current_ = node;
    if (current_ == root_.get()) {
        if (rootMessage_) emit(rootMessage_());
    } else if (current_->hasChildren()) {
        setChildrenMessage();
    } else if (current_->func) {
        current_->func();
        if (current_->hasChildren()) setChildrenMessage();
        else setMenuDataMessage();
    } else {
        emit("Error!!");
    }
}

// --- message formatting (ports set_children_message / set_*_menu_data_message)-

void MenuTree::setChildrenMessage() {
    if (!current_->hasChildren()) return;
    MenuNode* child = current_->currentChildNode();
    std::string msg;
    if (current_->parent && current_->parent != root_.get()) {
        msg = current_->parent->name + ": - " + current_->name + ": - " + child->name;
    } else {
        msg = current_->name + ": - " + child->name;
    }
    emit(msg);
}

void MenuTree::setMenuDataMessage() {
    if (current_->dataItems.empty()) return;
    if (current_->dataPosition < 0) current_->dataPosition = 0;
    const std::string& item = current_->dataItems[current_->dataPosition];

    const bool parentIsReal = current_->parent && current_->parent != root_.get();
    const bool grandparentIsReal =
        parentIsReal && current_->parent->parent && current_->parent->parent != root_.get();

    std::string msg;
    if (grandparentIsReal) {
        msg = current_->parent->parent->name + ": - " + current_->parent->name + ": - " +
              current_->dataPrompt + " - " + item;
    } else if (parentIsReal) {
        msg = current_->parent->name + ": - " + current_->dataPrompt + " - " + item;
    } else {
        msg = current_->dataPrompt + " - " + item;
    }
    emit(msg);
}

}  // namespace mc
