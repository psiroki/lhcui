#include "hui/KeyRepeatDriver.h"

namespace hui {

bool KeyRepeatDriver::shouldRepeat(Button b) {
    switch (b) {
        case Button::Up:
        case Button::Down:
        case Button::Left:
        case Button::Right:
        case Button::L1:
        case Button::L2:
        case Button::R1:
        case Button::R2:
            return true;
        default:
            return false;
    }
}

void KeyRepeatDriver::onButtonDown(Button b) {
    if (!shouldRepeat(b)) return;
    size_t idx = static_cast<size_t>(b);
    if (idx < held_.size()) {
        held_[idx] = HeldButton{ b, 0.0f, 0.0f, false };
    }
}

void KeyRepeatDriver::onButtonUp(Button b) {
    size_t idx = static_cast<size_t>(b);
    if (idx < held_.size()) {
        held_[idx].reset();
    }
}

void KeyRepeatDriver::flushHeld() {
    for (auto& slot : held_) {
        slot.reset();
    }
}

} // namespace hui
