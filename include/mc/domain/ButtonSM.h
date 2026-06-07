#pragma once
//
// ButtonSM — a footswitch state machine. Ports EffectLoops.ButtonOnPedalBoard:
// a short press (< 0.5s) yields the button's primary action; a longer hold
// yields its long-press action; a partner (double-press) within 0.25s yields the
// "partner func". Timing comes from IClock so it is fully testable.
//
// Edges use active-low pin values to match the hardware (and the Python
// `int_capture_pin_val`): false == pressed, true == released.
//
// Note: in the sim, scripted input delivers already-classified events, so this
// SM is exercised directly by unit tests and is ready for a real GPIO adapter in
// Phase 3.
//
#include <string>

#include "mc/ports/IClock.h"

namespace mc {

class ButtonSM {
public:
    enum class PressKind { None, Short, Long, Partner };
    struct Result {
        PressKind kind = PressKind::None;
        std::string action;  // primary name, long-press func, or "partner func"
    };

    static constexpr double kLongPressThreshold = 0.5;  // seconds
    static constexpr double kPartnerWindow = 0.25;      // seconds

    ButtonSM(std::string name, std::string longPressFunc, const IClock& clock);

    // Bidirectional, like Python set_partner.
    void setPartner(ButtonSM* partner);

    // Feed a pin edge; returns the resulting action on release (None on press).
    Result onEdge(bool pinValue);

    bool isPressed() const { return isPressed_; }
    double lastActionTime() const { return lastActionTime_; }
    void setLastActionTime(double t) { lastActionTime_ = t; }
    void setPedalConfigChanged(bool v) { pedalConfigChanged_ = v; }
    bool pedalConfigChanged() const { return pedalConfigChanged_; }

    const std::string& name() const { return name_; }
    const std::string& longPressFunc() const { return longPressFunc_; }

private:
    const IClock& clock_;
    std::string name_;
    std::string longPressFunc_;
    ButtonSM* partner_ = nullptr;
    bool isPressed_ = false;
    bool pedalConfigChanged_ = false;
    double start_ = 0.0;
    double lastActionTime_ = 0.0;
};

}  // namespace mc
