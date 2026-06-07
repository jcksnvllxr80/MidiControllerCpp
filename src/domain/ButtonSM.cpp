#include "mc/domain/ButtonSM.h"

#include <utility>

namespace mc {

ButtonSM::ButtonSM(std::string name, std::string longPressFunc, const IClock& clock)
    : clock_(clock), name_(std::move(name)), longPressFunc_(std::move(longPressFunc)) {
    start_ = clock_.now();
    lastActionTime_ = start_;
}

void ButtonSM::setPartner(ButtonSM* partner) {
    if (partner) {
        partner_ = partner;
        partner_->partner_ = this;
    }
}

ButtonSM::Result ButtonSM::onEdge(bool pinValue) {
    Result r;
    if (!pinValue) {  // pressed (active low)
        isPressed_ = true;
        start_ = clock_.now();
        return r;  // None on press
    }

    // released
    const double dt = clock_.now() - start_;
    // Mirrors the Python partner gate: take our own short/long branch unless the
    // partner just changed config within the 0.25s window.
    bool ownBranch;
    if (partner_) {
        ownBranch = (!partner_->pedalConfigChanged_) ||
                    (clock_.now() - partner_->lastActionTime_ > kPartnerWindow);
    } else {
        ownBranch = true;
    }

    if (ownBranch) {
        if (dt < kLongPressThreshold) {
            r.kind = PressKind::Short;
            r.action = name_;
        } else {
            r.kind = PressKind::Long;
            r.action = longPressFunc_;  // secondary_function()
        }
    } else {
        r.kind = PressKind::Partner;
        r.action = "partner func";
        partner_->pedalConfigChanged_ = false;
    }
    isPressed_ = false;
    return r;
}

}  // namespace mc
