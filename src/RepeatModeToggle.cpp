#include "hui/RepeatModeToggle.h"

namespace hui {

RepeatModeToggle::RepeatModeToggle(RepeatMode mode)
    : mode_(mode) {}

void RepeatModeToggle::cycle() {
    switch (mode_) {
        case RepeatMode::Off:
            mode_ = RepeatMode::All;
            break;
        case RepeatMode::All:
            mode_ = RepeatMode::One;
            break;
        case RepeatMode::One:
            mode_ = RepeatMode::Off;
            break;
    }
}

void RepeatModeToggle::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    const bool disabled = isDisabled();
    const bool active = (mode_ != RepeatMode::Off) && !disabled;

    if (active) {
        renderer.fillRect(bounds_, theme.focusFillColor);
        renderer.drawRect(bounds_, theme.accent, 1);
    } else {
        renderer.fillRect(bounds_, theme.surfaceAlt);
    }

    const char* text = "🔁 OFF";
    if (mode_ == RepeatMode::All) {
        text = "🔁 ALL";
    } else if (mode_ == RepeatMode::One) {
        text = "🔂 ONE";
    }

    Color col = disabled ? theme.textDisabled : (active ? theme.accent : theme.textSecondary);

    Size sz = renderer.measureText(text, theme.fontSmall);
    int textX = bounds_.x + (bounds_.w - sz.w) / 2;
    int textY = bounds_.y + (bounds_.h - sz.h) / 2;
    renderer.drawText(text, {textX, textY}, theme.fontSmall, col);
}

} // namespace hui
