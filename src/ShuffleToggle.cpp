#include "hui/ShuffleToggle.h"

namespace hui {

ShuffleToggle::ShuffleToggle(bool shuffle)
    : shuffle_(shuffle) {}

void ShuffleToggle::draw(IRenderer& renderer, const Theme& theme) {
    if (bounds_.w <= 0 || bounds_.h <= 0) {
        return;
    }

    const bool active = shuffle_ && !isDisabled();

    if (active) {
        renderer.fillRect(bounds_, theme.focusFillColor);
        renderer.drawRect(bounds_, theme.accent, 1);
    } else {
        renderer.fillRect(bounds_, theme.surfaceAlt);
    }

    const char* text = active ? "🔀 SHUFFLE" : "🔀";
    Color col = isDisabled() ? theme.textDisabled : (active ? theme.accent : theme.textSecondary);

    Size sz = renderer.measureText(text, theme.fontSmall);
    int textX = bounds_.x + (bounds_.w - sz.w) / 2;
    int textY = bounds_.y + (bounds_.h - sz.h) / 2;
    renderer.drawText(text, {textX, textY}, theme.fontSmall, col);
}

} // namespace hui
